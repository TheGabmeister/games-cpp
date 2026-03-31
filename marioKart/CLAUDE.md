# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A C++17 OpenGL 3.3 game built on a platform/game-layer split. The platform layer handles windowing (GLFW), OpenGL setup (GLAD), input, and the main loop. Game code lives entirely in three entry points: `initGame()`, `gameLogic()`, `closeGame()`.

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
- `logs.cpp` — file + console logging with timestamps
- `errorReporting.cpp` — OpenGL debug output callback
- `platformTools.cpp` — assertion helpers (`permaAssert`), `defer()` RAII macro
- Do not modify platform code unless the task requires it.

**Game layer** (`src/gameLayer/`, `include/gameLayer/`):
- `gameLayer.cpp` — implements the three entry points
- `gameLayer.h` — declares entry points and exposes platform API to game code

```cpp
bool initGame();                                          // called once at startup
bool gameLogic(float deltaTime, platform::Input &input);  // called every frame; return false to quit
void closeGame();                                         // called on shutdown (not guaranteed on force-close)
```

**Adding new files:** Place `.cpp` in `src/gameLayer/`, `.h` in `include/gameLayer/`. CMake auto-discovers via `file(GLOB_RECURSE)`. When the IDE prompts "Add to CMake?" always say **NO**.

## Libraries (in `thirdparty/`)

| Library | Purpose |
|---------|---------|
| GLFW 3.4 | Windowing and input |
| GLAD | OpenGL 3.3 function loader |
| glm 1.0.3 | Math (vectors, matrices) |
| stb_image | Image loading |
| stb_truetype | TTF font loading |
| raudio | Audio playback (miniaudio-based, built with `RAUDIO_STANDALONE`) |

## Input System

Access via `platform::Input &input` in `gameLogic()`:
- `input.isButtonHeld/Pressed/Released/Typed(platform::Button::X)` for keyboard
- `input.lMouse`, `input.rMouse` for mouse buttons
- `input.mouseX`, `input.mouseY` for mouse position
- `input.deltaTime` for frame time
- `input.typedInput` for text input characters
- `input.controller` for gamepad

## Platform Utilities (`platform::` namespace, declared in `gameLayer.h`)

- `writeEntireFile()` / `readEntireFile()` / `appendToFile()` — binary file I/O
- `setFullScreen()` / `isFullScreen()` — fullscreen toggling
- `getFrameBufferSize()` — use for `glViewport` (may differ from window size on HiDPI)
- `getWindowSize()` — logical window dimensions
- `showMouse()` / `getRelMousePosition()` / `setRelMousePosition()`

## Rendering

Currently uses raw OpenGL calls with custom vertex/fragment shaders (`resources/vertex.vert`, `resources/fragment.frag`). No 2D renderer library is linked.

## MSVC-Specific

- AVX2 SIMD enabled via `/arch:AVX2`
- Static runtime linking (MultiThreaded / MultiThreadedDebug)
- Console window suppressed via `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`
- Unicode force-disabled
