# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A C++17 OpenGL 3.3 kart-racing game built on a platform/game-layer split. The platform layer handles windowing (GLFW), OpenGL setup (GLAD), input, and the main loop. Game code lives entirely in three entry points: `initGame()`, `gameLogic()`, `closeGame()`.

The CMake target is named `mariokart`.

**Design reference:** `SPEC.md` contains the full game design spec — mechanics, controls, track design, item system, AI behavior, and phased development roadmap. Consult it for gameplay intent and implementation priorities.

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
- `stringManipulation.cpp` — `toLower`, `toUpper`, `split`, `strlcpy` utilities
- Do not modify platform code unless the task requires it.

**Game layer** (`src/gameLayer/`, `include/gameLayer/`):
- `gameLayer.cpp` — implements the three entry points, owns the fixed-step simulation loop, renders the 3D world and 2D HUD overlay
- `gameLayer.h` — declares entry points and exposes platform API to game code
- `gameState.cpp` / `gameState.h` — game state structs, input processing, player physics, AI logic, checkpoint/lap tracking, ranking, camera
- `trackSystems.cpp` / `trackSystems.h` — track construction (`buildTrack`), centerline sampling (`sampleTrackPosition`, `sampleTrackForward`), track queries (`queryTrackPosition`), kart transform helpers, distance wrapping utilities
- `itemSystems.cpp` / `itemSystems.h` — item box pickup/respawn, item rolling (weighted by race position), item use (mushroom/banana/green shell/red shell), projectile update with wall bouncing and homing, hazard collision, cleanup
- `gameEvents.cpp` / `gameEvents.h` — lightweight synchronous event queue
- `renderer.cpp` / `renderer.h` — 3D primitive renderer (quads, boxes, lines, markers)
- `gameConfig.h` — game-tunable constants (window settings, simulation rate, physics, drift, boost, items, AI)

```cpp
bool initGame();                                          // called once at startup
bool gameLogic(float deltaTime, platform::Input &input);  // called every frame; return false to quit
void closeGame();                                         // called on shutdown (not guaranteed on force-close)
```

**Adding new files:** Place `.cpp` in `src/gameLayer/`, `.h` in `include/gameLayer/`. CMake auto-discovers via `file(GLOB_RECURSE)`. When the IDE prompts "Add to CMake?" always say **NO**.

## Game State Architecture

`GameState` is the top-level container, composed of:

- `RaceState` — phase (`Boot`/`Countdown`/`Racing`/`Finished`), lap count, countdown/race timers, ranking
- `TrackState` — centerline waypoints, checkpoints, road/wall width, bounds, boost pads, item boxes
- `CameraState` — chase-camera position, target, distance, height
- `DebugState` — overlay toggle, event flash timer
- `std::vector<KartState>` — per-kart state: position, velocity, heading, speed, drift state, boost timer, held item, spinout timer, checkpoint progress, control type (`Player`/`AI`), off-road/wrong-way flags
- `std::vector<Projectile>` — active shells (green/red) with lifetime, wall bouncing, homing
- `std::vector<Hazard>` — dropped bananas on the track
- `EventQueue` — frame-local event notifications (cleared each step)

Key update flow: `createDefaultGameState()` at init → each frame: `processGameInput()` once (one-shot actions + player controls), then `updateGameScaffold()` N times at fixed 60 Hz steps → `updateItemSystems()` handles item/projectile/hazard logic → systems read/write `GameState` directly, emitting events for discrete transitions.

## Event System

Lightweight synchronous queue on `GameState::events`. Events are frame-local notifications, not a subscription system. Current types: `RaceStarted`, `RaceFinished`, `CheckpointPassed`, `LapCompleted`, `KartHitWall`, `RespawnRequested`, `DriftBoosted`, `BoostPadHit`, `AIRecovered`, `ItemPickedUp`, `ItemUsed`, `KartHitHazard`, `KartHitProjectile`.

Flow: clear at step start → gameplay systems push events as things happen → HUD/debug reads them → discarded next step.

## Simulation

The game uses a **fixed-step simulation** at 60 Hz (`FIXED_DT` in `gameConfig.h`). Input is collected once per frame, then the simulation runs 1+ steps to catch up. Frame time is clamped to `MAX_FRAME_TIME` (100ms) to prevent spiral-of-death after debugger pauses.

Input handling is split from simulation: `processGameInput()` handles one-shot actions (key presses, toggles) and writes player intent into `KartInputState`. `updateGameScaffold()` is a pure simulation step with no `platform::Input` dependency — safe to call multiple times per frame.

**Player kart** uses free 2D driving physics (acceleration, braking, steering, drag, drift with hop/mini-turbo) with `gameConfig.h` constants. **AI karts** follow the track centerline with oscillating lane offsets, corner speed reduction, and rubber-banding relative to the player. The player's position is projected onto the track via `queryTrackPosition()` for checkpoint and ranking calculations.

**Drift system**: LeftShift + steering initiates a hop → drift. Drift locks turn direction with bias, adds lateral slip. Releasing after 0.8s+ grants a mini-turbo speed boost. Wall hits cancel drift.

**Item system**: 8 item boxes on track, position-weighted random items (mushroom/banana/green shell/red shell). Space key to use. AI uses items after a 2s delay. Green shells bounce off walls (3x), red shells home toward the kart ahead. All hits cause a spinout (rotation + speed loss).

**Track systems** (in `trackSystems.cpp`): wall collision pushes the kart back to `wallHalfWidth` from the centerline and reduces speed. Off-road detection triggers when lateral distance exceeds `roadHalfWidth`, reducing max speed and acceleration. Wrong-way detection compares kart heading to track forward direction over a sustained period.

## Libraries (in `thirdparty/`)

| Library | Purpose |
|---------|---------|
| GLFW 3.4 | Windowing and input |
| GLAD | OpenGL 3.3 function loader |
| glm 1.0.3 | Math (vectors, matrices) |
| stb_image | Image loading |
| stb_truetype | TTF font loading |
| raudio | Audio playback (miniaudio-based, built with `RAUDIO_STANDALONE`); currently unused |

## Input System

Access via `platform::Input &input` in `gameLogic()`:
- `input.isButtonHeld/Pressed/Released/Typed(platform::Button::X)` for keyboard
- `input.lMouse`, `input.rMouse` for mouse buttons
- `input.mouseX`, `input.mouseY` for mouse position
- `input.deltaTime` for frame time
- `input.typedInput` for text input characters (20-char buffer, auto-repeat at 480ms initial / 70ms repeat)
- `input.controller` for gamepad (analog sticks, triggers, digital buttons)

Current game controls: Up/Down = accelerate/brake, Left/Right = steer, LeftShift = drift, Space = use item, Tab = toggle HUD, Enter = reset race.

## Platform Utilities (`platform::` namespace, declared in `gameLayer.h`)

- `writeEntireFile()` / `readEntireFile()` / `appendToFile()` — binary file I/O
- `setFullScreen()` / `isFullScreen()` — fullscreen toggling
- `getFrameBufferSize()` — use for `glViewport` (may differ from window size on HiDPI)
- `getWindowSize()` — logical window dimensions
- `showMouse()` / `getRelMousePosition()` / `setRelMousePosition()`
- `platform::log()` — log to file (`logs.txt`) and console with timestamps

## Rendering

**3D renderer** (`renderer.h`/`renderer.cpp`): minimal primitive renderer using the vertex/fragment shaders in `resources/`. API:
- `renderer::init()` / `renderer::close()` — shader compilation, VAO/VBO lifecycle
- `renderer::beginFrame(camera, w, h)` — builds view/projection from `CameraState`, enables depth test
- `renderer::drawQuad(center, size, color, rotationY)` — flat XZ-plane quad
- `renderer::drawBox(center, size, color, rotationY)` — 3D box with optional Y rotation
- `renderer::drawLine(start, end, color)` / `renderer::drawMarker(pos, size, color)` — debug primitives

Shaders: `vertex.vert` transforms `vec3` positions by `u_mat` (projection * view * model); `fragment.frag` outputs flat `u_color`. The 2D HUD overlay uses a scissor+glClear trick (no geometry), rendered after disabling depth test.

## Serialization Warning

`resources/gameData.data` is a binary save file. Changing the layout of persisted structs will invalidate existing saved data.

## MSVC-Specific

- AVX2 SIMD enabled via `/arch:AVX2`
- Static runtime linking (MultiThreaded / MultiThreadedDebug)
- Console window suppressed via `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`
- Unicode force-disabled
