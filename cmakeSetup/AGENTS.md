# AGENTS.md

Guidance for coding agents working in this repository.

## Project Overview

This is an OpenGL 2D game template in C++17, based on
`meemknight/cmakeSetup`. It uses GLFW for windowing, glad for OpenGL loading,
gl2d for 2D rendering, Dear ImGui docking for debug UI, and raudio for audio.

## Build Commands

Run commands from the repository root.

```sh
cmake -B build
cmake --build build --config Debug
cmake --build build --config Release
```

The executable is written under `build/Debug/` or `build/Release/`. The
`resources/` directory is copied next to the executable by a post-build step.

## Architecture

The code is split into a platform layer and a game layer.

- `src/platform/` and `include/platform/`: GLFW main loop, input, file I/O,
  ImGui setup, logging, profiling, and platform error handling. Treat this as
  the engine/platform layer and avoid changing it unless the task requires it.
- `src/gameLayer/` and `include/gameLayer/`: game-specific code. Prefer putting
  gameplay changes here.

The platform layer calls the game layer through the entry points declared in
`include/gameLayer/gameLayer.h`:

- `initGame()`: called once at startup.
- `gameLogic(float deltaTime, platform::Input &input)`: called every frame;
  return `false` to quit.
- `closeGame()`: called on shutdown, though force-close paths may skip it.

## CMake And Source Layout

- The project uses CMake 3.16+ and C++17.
- Source files under `src/*.cpp` are discovered with `GLOB_RECURSE`; do not add
  new `.cpp` files to `CMakeLists.txt` by hand.
- Public headers live under `include/`.
- Third-party libraries live in `thirdparty/` and have their own CMake files.
- ENet is present but disabled in the root `CMakeLists.txt`; enabling it
  requires uncommenting both its `add_subdirectory` and `target_link_libraries`
  entries.

## Important Compile-Time Defines

- `RESOURCES_PATH` is `"./resources/"` and should be used as the prefix for
  asset paths.
- `PRODUCTION_BUILD` and `DEVELOPLEMT_BUILD` are set by configuration. The
  spelling of `DEVELOPLEMT_BUILD` is intentional and should not be corrected
  casually.
- `REMOVE_IMGUI` is defined in platform code. Set it to `1` only when the task
  is specifically about stripping ImGui.

## ImGui Notes

The project uses Dear ImGui `v1.92.7-docking`. The local
`thirdparty/imgui-docking/` directory also contains project-used addons such as
`imguiThemes.h`, `IconsForkAwesome.h`, `imfilebrowser.h`, and
`multiPlot.h`/`multiPlot.cpp`.

When touching ImGui code or upgrading ImGui, watch for docking API changes and
local addon compatibility.

## Coding Guidelines

- Prefer existing project patterns over introducing new abstractions.
- Keep gameplay changes in the game layer when possible.
- Keep platform changes small and deliberate.
- Use `RESOURCES_PATH` for assets that need to load at runtime.
- Avoid broad formatting churn in third-party code.
- Do not edit generated build output under `build/` unless explicitly asked.
- Preserve existing misspellings in macros, target names, and compatibility
  surfaces unless the task is explicitly to rename them.

## Verification

For CMake changes or C++ changes, at minimum run:

```sh
cmake --build build --config Debug
```

If the build directory is missing or stale, configure first:

```sh
cmake -B build
```

Known benign warnings include:

- `APIENTRY` macro redefinition between glad and the Windows SDK.
- CMake CMP0115 development warnings from `stb_image` and `stb_truetype`.

