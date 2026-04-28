# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OpenGL 2D game template (C++17) based on [meemknight/cmakeSetup](https://github.com/meemknight/cmakeSetup). Uses GLFW for windowing, glad for OpenGL loading, gl2d for 2D rendering, Dear ImGui (docking branch) for debug UI, and raudio for audio.

## Build Commands

```bash
# Configure (from repo root)
cmake -B build

# Build Debug
cmake --build build --config Debug

# Build Release (enables LTO)
cmake --build build --config Release
```

The executable is output to `build/Debug/` or `build/Release/`. A post-build step copies `resources/` next to the executable automatically.

## Architecture

**Two-layer design: platform vs game logic.**

- `src/platform/` + `include/platform/` — GLFW main loop, input handling, file I/O, ImGui setup, error reporting, logging. This is the engine layer; you rarely need to edit it.
- `src/gameLayer/` + `include/gameLayer/` — Game-specific code. `gameLayer.h` defines the three entry points the platform layer calls:
  - `initGame()` — called once at startup
  - `gameLogic(float deltaTime, platform::Input &input)` — called every frame; return `false` to quit
  - `closeGame()` — called on shutdown (not guaranteed on force-close)

New gameplay code goes in the game layer. The platform layer owns the main loop and feeds input/timing into `gameLogic`.

## Key Compile-Time Macros

- `RESOURCES_PATH` — always `"./resources/"` (relative to executable). Used as a prefix for asset loading: `RESOURCES_PATH "myfile.png"`.
- `PRODUCTION_BUILD` / `DEVELOPLEMT_BUILD` (note: typo is intentional, kept for compatibility) — set automatically based on Debug/Release config. Controls debug console allocation on Windows and assert behavior.
- `REMOVE_IMGUI` — defined in `platformTools.h` and `glfwMain.cpp`. Set to `1` to strip ImGui entirely.

## Conventions

- Sources are auto-discovered via `GLOB_RECURSE` on `src/*.cpp` — no need to register new files in CMake.
- All thirdparty dependencies live in `thirdparty/` with their own CMakeLists.txt and are linked statically.
- ENet (networking) is included but commented out in CMakeLists.txt — uncomment both the `add_subdirectory` and the `target_link_libraries` line to enable it.
- Save data uses raw struct serialization via `platform::readEntireFile` / `platform::writeEntireFile` into the resources directory.
