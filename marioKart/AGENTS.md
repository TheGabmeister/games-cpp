# AGENTS.md

## Purpose

This repository is a small C++17/CMake OpenGL game template centered around GLFW, GLAD, audio support, and a lightweight platform layer. Use this file as the working guide for coding agents and contributors.

## Repo layout

- `src/gameLayer/`: game-specific logic. This is the main place to add or change gameplay code.
- `include/gameLayer/`: headers for game-layer code.
- `src/platform/`: windowing, input, startup, file I/O helpers, logging, and other platform code.
- `include/platform/`: platform-layer headers.
- `resources/`: runtime assets loaded through `RESOURCES_PATH`.
- `thirdparty/`: vendored dependencies. Do not modify these unless the task explicitly requires it.

## Main entry points

- `src/platform/glfwMain.cpp`: application startup, GLFW/OpenGL setup, the main loop, fullscreen handling, and platform file helpers.
- `src/gameLayer/gameLayer.cpp`: game-layer implementation. This is where `initGame()`, `gameLogic()`, and `closeGame()` live.
- `include/gameLayer/gameLayer.h`: public game-layer declarations plus the platform functions exposed to gameplay code.

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
- If you change `PRODUCTION_BUILD` in `CMakeLists.txt`, rebuild from a clean build directory.

## Project-specific rules

- New `.cpp` files belong under `src/`; new headers belong under `include/`.
- Do not manually add source files to `CMakeLists.txt`. The project uses `file(GLOB_RECURSE ... src/*.cpp)` and picks them up automatically.
- Prefer gameplay changes in `src/gameLayer/` before touching the platform layer.
- Treat `thirdparty/` as read-only unless dependency work is the explicit task.
- Keep asset loads using `RESOURCES_PATH` instead of hard-coded paths.
- Preserve the current split between game-layer code and platform-layer code. Prefer gameplay changes in `src/gameLayer/` before touching platform code.

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
- `platform::writeEntireFile()` / `platform::readEntireFile()` / `platform::appendToFile()`

## Validation

- There are no dedicated automated tests in this repo today.
- Minimum validation for code changes is a successful CMake configure and build.
- For gameplay or rendering edits, also do a quick smoke run of the app when possible.

## Notes for agents

- Preserve the existing C++ style unless the task asks for a broader cleanup.
- Keep changes scoped; avoid incidental refactors in platform or third-party code.
- If you add files and the IDE offers to edit CMake source lists automatically, decline it.
- Be careful with serialized data in `resources/gameData.data`; changing the layout of persisted structs can invalidate existing saved data.
- `thirdparty/` contains vendored libraries such as GLFW, GLAD, glm, stb, and raudio. Treat them as read-only unless the task explicitly targets dependency code.
- On MSVC builds, expect `/arch:AVX2`, static runtime linking, `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`, and Unicode to be force-disabled in `CMakeLists.txt`.
