# AGENTS.md

## Purpose

This repository is a small C++/CMake game template centered around GLFW, OpenGL, audio support, and a lightweight platform layer. Use this file as the working guide for coding agents and contributors.

## Repo layout

- `src/gameLayer/`: game-specific logic. This is the main place to add or change gameplay code.
- `include/gameLayer/`: headers for game-layer code.
- `src/platform/`: windowing, input, startup, file I/O helpers, logging, and other platform code.
- `include/platform/`: platform-layer headers.
- `resources/`: runtime assets loaded through `RESOURCES_PATH`.
- `thirdparty/`: vendored dependencies. Do not modify these unless the task explicitly requires it.

## Main entry points

- `src/platform/glfwMain.cpp`: application startup, GLFW/OpenGL setup, main loop, fullscreen handling, and platform file helpers.
- `src/gameLayer/gameLayer.cpp`: `initGame()`, `gameLogic()`, and `closeGame()`.
- `include/gameLayer/gameLayer.h`: public game-layer declarations and platform API surface used by gameplay code.

## Build and run

Recommended local workflow from the repo root:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

The executable target name is `mygame`.

On single-config generators, run the output from `build/`. On Visual Studio or other multi-config generators, use the built `Debug` or `Release` output directory for the chosen configuration.

## Project-specific rules

- New `.cpp` files belong under `src/`; new headers belong under `include/`.
- Do not manually add source files to `CMakeLists.txt`. The project uses `file(GLOB_RECURSE ... src/*.cpp)` and picks them up automatically.
- Prefer gameplay changes in `src/gameLayer/` before touching the platform layer.
- Treat `thirdparty/` as read-only unless dependency work is the explicit task.
- Keep asset loads using `RESOURCES_PATH` instead of hard-coded paths.
- `PRODUCTION_BUILD` in `CMakeLists.txt` changes asset-path behavior and build settings. If you change it, expect to regenerate or rebuild from a clean build directory.

## Validation

- There are no dedicated automated tests in this repo today.
- Minimum validation for code changes is a successful CMake configure and build.
- For gameplay or rendering edits, also do a quick smoke run of the app when possible.

## Notes for agents

- Preserve the existing C++ style unless the task asks for a broader cleanup.
- Keep changes scoped; avoid incidental refactors in platform or third-party code.
- If you add files and the IDE offers to edit CMake source lists automatically, decline it.
- Be careful with serialized data in `resources/gameData.data`; changing the layout of persisted structs can invalidate existing saved data.
