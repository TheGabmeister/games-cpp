# AGENTS.md

## Purpose

This repository is a small C++17/CMake OpenGL kart-racing prototype built around GLFW, GLAD, gl2d/glui, and a lightweight platform layer. Use this file as the working guide for coding agents and contributors.

## Repo layout

- `src/gameLayer/`: gameplay code. Prefer adding or changing kart, race, track, UI, and item behavior here first.
- `include/gameLayer/`: headers for game-layer systems and shared gameplay state.
- `src/platform/`: startup, windowing, input collection, logging, OpenGL error reporting, file helpers, and monitor/fullscreen utilities.
- `include/platform/`: platform-layer headers.
- `resources/`: runtime assets loaded through `RESOURCES_PATH`.
  - `resources/vertex.vert` / `resources/fragment.frag`: shaders used by the primitive 3D renderer.
  - `resources/roboto_black.ttf`, `resources/arial.ttf`, `resources/fontawesome-webfont.ttf`: font assets.
  - `resources/target.ogg`: sample audio asset.
  - `resources/gameData.data`: binary runtime/save data. Treat persisted layouts carefully.
- `thirdparty/`: vendored dependencies. Do not modify these unless the task explicitly requires it.
- `CLAUDE.md`: supplementary repo notes. It is useful context, but the codebase itself is the final source of truth.

There is currently no `SPEC.md` in the repository. If the user provides a spec separately, follow that; otherwise, use the implemented code and this guide as the baseline for scope and architecture.

## Main entry points

- `src/platform/glfwMain.cpp`: application startup, GLFW/OpenGL setup, callbacks, main loop, fullscreen handling, and platform file helpers.
- `src/gameLayer/gameLayer.cpp`: game-layer implementation. This is where `initGame()`, `gameLogic()`, and `closeGame()` live, along with menu/HUD/world rendering.
- `include/gameLayer/gameLayer.h`: public game-layer declarations plus the platform functions exposed to gameplay code.

Current important gameplay support files:

- `include/gameLayer/gameState.h` / `src/gameLayer/gameState.cpp`: persistent gameplay state, menu/race flow, player physics, AI updates, lap/checkpoint tracking, camera updates, and high-level simulation orchestration.
- `include/gameLayer/gameEvents.h` / `src/gameLayer/gameEvents.cpp`: lightweight frame-local event queue and event type definitions.
- `include/gameLayer/gameConfig.h`: centralized gameplay constants and tuning values.
- `include/gameLayer/renderer.h` / `src/gameLayer/renderer.cpp`: primitive 3D renderer, shader setup, and helpers such as `drawQuad`, `drawBox`, `drawLine`, and `drawMarker`.
- `include/gameLayer/trackSystems.h` / `src/gameLayer/trackSystems.cpp`: track construction, waypoint sampling, checkpoint math, track queries, wall/off-road checks, and kart transform helpers.
- `include/gameLayer/itemSystems.h` / `src/gameLayer/itemSystems.cpp`: item box pickup/respawn, item rolls, item use, projectile updates, and hazard cleanup.

Current important platform support files:

- `include/platform/platformInput.h` / `src/platform/platformInput.cpp`: keyboard, mouse, controller, and typed-input state tracking.
- `include/platform/logs.h` / `src/platform/logs.cpp`: file and console logging through `platform::log()` and `LogManager`.
- `include/platform/errorReporting.h` / `src/platform/errorReporting.cpp`: OpenGL debug output callback setup.
- `include/platform/otherPlatformFunctions.h` / `src/platform/otherPlatformFunctions.cpp`: monitor selection helper for fullscreen transitions.
- `include/platform/platformTools.h` / `src/platform/platformTools.cpp`: assertion helpers and small platform utilities.
- `include/platform/stringManipulation.h` / `src/platform/stringManipulation.cpp`: string helpers used by the platform layer.

The main gameplay API is:

```cpp
bool initGame();
bool gameLogic(float deltaTime, platform::Input &input);
void closeGame();
```

`gameLogic()` runs every frame and returns `false` to quit.

## Build and run

Recommended local workflow from the repo root:

```powershell
cmake -S . -B build
cmake --build build
```

If you want an explicit Visual Studio configuration, use:

```powershell
cmake --build build --config Debug
```

The CMake project and executable target are currently named `mariokart`.

On single-config generators, run the output from `build/`. On Visual Studio or other multi-config generators, use the built `Debug` or `Release` output directory for the chosen configuration.

## Build macros and configuration

- `RESOURCES_PATH` points to the absolute `resources/` directory in development builds and `./resources/` in production builds. Always use this macro for asset paths.
- `PRODUCTION_BUILD` changes asset-path behavior, enables LTO, and removes the console window on MSVC builds.
- `DEVELOPLEMT_BUILD` is also defined in the project. The typo is intentional in the codebase, so do not "fix" it unless the task explicitly requires a coordinated cleanup.
- `GLFW_INCLUDE_NONE=1` is defined so GLAD owns OpenGL loading.
- If you change `PRODUCTION_BUILD` in `CMakeLists.txt`, rebuild from a clean build directory.

## Project-specific rules

- New `.cpp` files belong under `src/`; new headers belong under `include/`.
- Do not manually add source files to `CMakeLists.txt`. The project uses `file(GLOB_RECURSE ... src/*.cpp)` and picks them up automatically.
- Prefer gameplay changes in `src/gameLayer/` before touching the platform layer.
- Treat `thirdparty/` as read-only unless dependency work is the explicit task.
- Keep asset loads using `RESOURCES_PATH` instead of hard-coded paths.
- Preserve the current split between game-layer code and platform-layer code.
- Be careful with serialized data in `resources/gameData.data`; changing the layout of persisted structs can invalidate existing saved data.

## Input and platform helpers

Gameplay receives per-frame input through `platform::Input &input` in `gameLogic()`.

- Keyboard helpers: `input.isButtonHeld(...)`, `input.isButtonPressed(...)`, `input.isButtonReleased(...)`, `input.isButtonTyped(...)`
- Mouse helpers: `input.lMouse`, `input.rMouse`, `input.mouseX`, `input.mouseY`
- Frame timing: `input.deltaTime`
- Text input: `input.typedInput`
- Controller state: `input.controller`

Useful platform functions exposed through `gameLayer.h` include:

- `platform::setFullScreen()` / `platform::isFullScreen()`
- `platform::getWindowSize()` / `platform::getFrameBufferSize()`
- `platform::setRelMousePosition()` / `platform::getRelMousePosition()`
- `platform::showMouse()`
- `platform::hasFocused()` / `platform::mouseMoved()`
- `platform::writeEntireFile()` / `platform::readEntireFile()` / `platform::appendToFile()` / `platform::getFileSize()`
- `platform::log()`

## Current gameplay scaffold

- The repo is no longer the original sample rectangle demo. It is a kart-racing prototype with a working menu-to-race loop.
- The outer frame loop is still owned by `src/platform/glfwMain.cpp`.
- `src/gameLayer/gameLayer.cpp` owns a fixed-step simulation accumulator and renders menus, the 3D world, and the HUD.
- The current race flow is `MainMenu -> KartSelect -> Countdown -> Racing -> Finished`.
- The player can choose from `KART_PALETTE_COUNT` kart colors before starting; the player color swaps with an AI color if needed.
- The track is waypoint-based and exposed through `trackSystems.*`, including checkpoints, boost pads, item boxes, and distance queries.
- The player uses free-driving physics with acceleration, braking, steering, drifting, mini-turbo boosts, off-road slowdown, wrong-way detection, and respawn recovery.
- AI karts follow the sampled track, vary their lane offset, slow for corners, use light rubber-banding, and recover if they get stuck.
- The item system currently supports mushrooms, bananas, green shells, and red shells, plus item-box respawn and simple race-position weighting.
- Rendering is split between the primitive 3D renderer in `renderer.*` and the 2D HUD/menu rendering done with gl2d/glui in `gameLayer.cpp`.
- `Tab` toggles the HUD/debug overlay during gameplay.

## Validation

- There are no dedicated automated tests in this repo today.
- Minimum validation for code changes is a successful CMake configure and build.
- For gameplay or rendering edits, also do a quick smoke run of the app when possible.

## Notes for agents

- Preserve the existing C++ style unless the task asks for a broader cleanup.
- Keep changes scoped; avoid incidental refactors in platform or third-party code.
- If you add files and the IDE offers to edit CMake source lists automatically, decline it.
- Keep AGENTS guidance aligned with the current codebase. If you substantially change architecture or workflow expectations, update this file in the same task when appropriate.
- `thirdparty/` contains vendored libraries such as GLFW, GLAD, glm, stb, gl2d, glui, and raudio. Treat them as read-only unless the task explicitly targets dependency code.
- On MSVC builds, expect `/arch:AVX2`, static runtime linking, `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`, and Unicode to be force-disabled in `CMakeLists.txt`.
