# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A C++17 OpenGL 3.3 kart-racing game built on a platform/game-layer split. The platform layer handles windowing (GLFW), OpenGL setup (GLAD), input, and the main loop. Game code lives entirely in three entry points: `initGame()`, `gameLogic()`, `closeGame()`.

The CMake target is named `mariokart`.

## Build

```bash
cmake -S . -B build
cmake --build build
```

In Visual Studio: open the folder, Ctrl+S on CMakeLists.txt to configure, select `mariokart.exe` from the startup dropdown, Ctrl+F5 to run.

**Production build:** Set `PRODUCTION_BUILD` to `ON` in CMakeLists.txt, delete the build folder, rebuild. This switches `RESOURCES_PATH` to relative paths, enables LTO, and removes the console window.

**No test framework.** Validate with a successful build and manual smoke run.

## Key Build Macros

- `RESOURCES_PATH` — absolute path to `resources/` in dev, `./resources/` in production. Always use this macro for asset paths.
- `PRODUCTION_BUILD` / `DEVELOPLEMT_BUILD` — the typo is intentional; do not fix unless explicitly asked.
- `GLFW_INCLUDE_NONE=1` — GLAD loads OpenGL, not GLFW.

## Architecture

**Platform layer** (`src/platform/`, `include/platform/`):
- `glfwMain.cpp` — `main()`, GLFW/OpenGL init, main loop, input collection, fullscreen handling, platform file I/O
- `platformInput.cpp` — keyboard (32 buttons), mouse, controller state tracking with press/held/release/typed semantics
- `logs.cpp` — file + console logging with timestamps via `LogManager`
- `errorReporting.cpp` — OpenGL debug output callback
- `platformTools.cpp` — assertion helpers (`permaAssert`), `defer()` RAII macro
- Do not modify platform code unless the task requires it.

**Game layer** (`src/gameLayer/`, `include/gameLayer/`):
- `gameLayer.cpp` — implements the three entry points, owns the `GameContext` (game state + rendering resources), renders the 3D world, 2D HUD (via gl2d/glui), and menu screens
- `gameState.cpp` / `gameState.h` — game state structs, input processing, player physics, AI logic, checkpoint/lap tracking, ranking, camera, menu phase transitions, out-of-bounds recovery
- `trackSystems.cpp` / `trackSystems.h` — track construction (`buildTrack`), centerline sampling, track queries (`queryTrackPosition` with segment-hint caching), kart transform helpers, distance wrapping utilities
- `itemSystems.cpp` / `itemSystems.h` — item box pickup/respawn, item rolling (weighted by race position), item use, projectile update with wall bouncing and homing, hazard collision, cleanup
- `gameEvents.cpp` / `gameEvents.h` — lightweight synchronous event queue (event types defined here)
- `renderer.cpp` / `renderer.h` — 3D primitive renderer (quads, boxes, lines, markers) with directional lighting
- `gameConfig.h` — game-tunable constants (window settings, simulation rate, physics, drift, boost, items, AI, menu, recovery)

```cpp
bool initGame();                                          // called once at startup
bool gameLogic(float deltaTime, platform::Input &input);  // called every frame; return false to quit
void closeGame();                                         // called on shutdown (not guaranteed on force-close)
```

**Adding new files:** Place `.cpp` in `src/gameLayer/`, `.h` in `include/gameLayer/`. CMake auto-discovers via `file(GLOB_RECURSE)`. When the IDE prompts "Add to CMake?" always say **NO**.

## Game State Architecture

`GameContext` (defined in `gameLayer.cpp`) owns the `GameState`, simulation accumulator, and rendering resources (`gl2d::Renderer2D`, `gl2d::Font`, `glui::RendererUi`). Draw functions receive game state and rendering resources as explicit parameters — no draw function reaches into globals.

`GameState` is the top-level simulation container, composed of:

- `MenuState` — kart selection slot index, preview rotation angle
- `RaceState` — phase, lap count, countdown/race timers, ranking
- `TrackState` — centerline waypoints, checkpoints, road/wall width, bounds, boost pads, item boxes, precomputed `segmentStartDistances`
- `CameraState` — chase-camera position, target, distance, height
- `DebugState` — overlay toggle, event flash timer
- `EntityState entities` — groups all simulation entities:
  - `std::vector<KartState>` — per-kart state (position, velocity, heading, speed, drift, boost, items, checkpoint progress, control type, `lastSegmentHint` for track query caching)
  - `std::vector<Projectile>` — active shells with lifetime, wall bouncing, homing
  - `std::vector<Hazard>` — dropped bananas on the track
- `EventQueue` — frame-local event notifications (cleared each step)

Key update flow: `createDefaultGameState()` at init → each frame: `processGameInput()` once (phase-dependent), then `updateGameScaffold()` N times at fixed 60 Hz → `updateItemSystems()` handles item/projectile/hazard logic → systems read/write `GameState` directly, emitting events for discrete transitions.

## Game Flow

```
MainMenu ──[Enter]──> KartSelect ──[Enter]──> Countdown ──> Racing ──> Finished
   ^                     |                                                |
   └──────[Escape]───────┘                        [Enter]────────────────┘
```

- **MainMenu**: Title text, kart color palette banner, pulsing "PRESS ENTER" prompt.
- **KartSelect**: 8 color swatches with selection highlight, 3D spinning kart preview, left/right to pick, enter to confirm, escape to go back. On confirm, player color is swapped with any AI that had the same color.
- **Countdown → Racing → Finished**: Standard race loop. After finish, enter returns to MainMenu.

Kart colors are shared via `getKartPaletteColor(int index)` — 8 colors used by both kart init and selection screen.

## Simulation

The game uses a **fixed-step simulation** at 60 Hz (`FIXED_DT` in `gameConfig.h`). Input is collected once per frame, then the simulation runs 1+ steps to catch up. Frame time is clamped to `MAX_FRAME_TIME` (100ms) to prevent spiral-of-death after debugger pauses.

Input handling is split from simulation: `processGameInput()` handles one-shot actions and writes player intent into `KartInputState`. `updateGameScaffold()` is a pure simulation step with no `platform::Input` dependency — safe to call multiple times per frame.

**Player kart** uses free 2D driving physics (acceleration, braking, steering, drag, drift with hop/mini-turbo) with `gameConfig.h` constants. **AI karts** follow the track centerline with oscillating lane offsets, corner speed reduction, and rubber-banding relative to the player.

**Track query optimization**: `queryTrackPosition()` accepts an optional `int *segmentHint` parameter. Each kart stores a `lastSegmentHint` that caches which track segment it was nearest to. The function searches a local window of segments around the hint first, only falling back to a full scan if the kart is far from the track. `TrackState::segmentStartDistances` (precomputed in `buildTrack`) eliminates per-query distance accumulation.

**Drift system**: LeftShift + steering initiates a hop → drift. Drift locks turn direction with bias, adds lateral slip. Releasing after 0.65s+ grants a mini-turbo speed boost. Wall hits cancel drift.

**Item system**: 8 item boxes on track, position-weighted random items (mushroom/banana/green shell/red shell). Space key to use. AI uses items after a 2s delay. Green shells bounce off walls (3x), red shells home toward the kart ahead. All hits cause a spinout.

**Track systems**: Wall collision pushes the kart back to `wallHalfWidth` from the centerline and reduces speed. Off-road detection triggers when lateral distance exceeds `roadHalfWidth`. Wrong-way detection compares kart heading to track forward direction over a sustained period.

**Out-of-bounds recovery**: the player's `lastSafePosition`/`lastSafeHeading` are saved only when on-road, not wrong-way, and moving forward. If OOB or stuck, respawn at the last safe pose with a brief freeze. AI karts have separate stuck detection that teleports them forward.

## Rendering

The game uses two rendering systems:

**3D renderer** (`renderer.h`/`renderer.cpp`): minimal primitive renderer with directional lighting. `renderer::beginFrame(camera, w, h)` builds view/projection, then `drawQuad`/`drawBox`/`drawLine`/`drawMarker` issue individual draw calls. Vertex data is interleaved (position + normal, 6 floats). Shaders in `resources/vertex.vert` and `resources/fragment.frag`.

**2D renderer** (gl2d + glui): used for menus, HUD, and text overlays. Text via `glui::renderText()`, rectangles via `renderer2d.renderRectangle()`. The 2D renderer has its own shader pipeline; after using it, the 3D shader must be re-bound via `renderer::beginFrame()` or `glUseProgram()`.

**Rendering order in `gameLogic()`**: clear screen → if menu phase, draw menu with gl2d/glui → else draw 3D world via `drawWorld3D()`, then HUD via `drawHud()`, then optional "RACE FINISHED" overlay.

## Input Controls

Up/Down = accelerate/brake, Left/Right = steer (also kart select navigation), LeftShift = drift, Space = use item, Tab = toggle HUD, Enter = confirm/reset, Escape = back (in menus).

Access via `platform::Input &input` in `gameLogic()`: `isButtonHeld/Pressed/Released/Typed(platform::Button::X)` for keyboard, `lMouse`/`rMouse` for mouse, `controller` for gamepad.

## Serialization Warning

`resources/gameData.data` is a binary save file. Changing the layout of persisted structs will invalidate existing saved data.

## MSVC-Specific

- AVX2 SIMD enabled via `/arch:AVX2`
- Static runtime linking (MultiThreaded / MultiThreadedDebug)
- Console window suppressed via `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`
- Unicode force-disabled
