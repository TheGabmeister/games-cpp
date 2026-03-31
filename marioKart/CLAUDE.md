# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A cross-platform C++17 OpenGL game template (by meemknight) providing windowing, input, 2D rendering, UI, audio, and save systems. Developers write game logic in `initGame()`, `gameLogic()`, and `closeGame()` — the platform layer handles everything else.

## Build System

CMake 3.16+ with auto-discovered sources (`file(GLOB_RECURSE)` over `src/*.cpp`).

**Build (Visual Studio on Windows):**
```
# Open folder in VS, Ctrl+S on CMakeLists.txt to configure, then Ctrl+F5 to build+run
# Select "mygame.exe" from the startup item dropdown
```

**Build (command line):**
```
cmake -B out -S .
cmake --build out
```

**Production build:** Set `PRODUCTION_BUILD` to `ON` in CMakeLists.txt, delete `out/` folder, rebuild. This switches `RESOURCES_PATH` to relative paths, enables LTO, and removes the console window.

**No test framework is configured.**

## Key Build Macros

- `RESOURCES_PATH` — absolute path to `resources/` in dev, `./resources/` in production. Always use this macro to reference assets.
- `PRODUCTION_BUILD` / `DEVELOPLEMT_BUILD` (note: typo is intentional in codebase) — toggles debug vs shipping behavior.
- `GLFW_INCLUDE_NONE=1` — GLAD loads OpenGL, not GLFW.

## Architecture

**Two-layer design:**
- **Platform layer** (`src/platform/`, `include/platform/`) — GLFW window, OpenGL init, input collection, main loop, ImGui setup. Do not modify unless changing platform behavior.
- **Game layer** (`src/gameLayer/`, `include/gameLayer/`) — where all game code goes. Three entry points in `gameLayer.h`:
  - `initGame()` — called once at startup
  - `gameLogic(float deltaTime, platform::Input &input)` — called every frame; return `false` to quit
  - `closeGame()` — called on shutdown (not guaranteed on force-close)

**Adding new files:** Place `.cpp` in `src/gameLayer/`, `.h` in `include/gameLayer/`. CMake auto-discovers them. When Visual Studio prompts "Add to CMake?" always say **NO**.

## Key Libraries (in `thirdparty/`)

| Library | Purpose |
|---------|---------|
| gl2d | 2D batch renderer (textures, shapes, text, cameras, particles) |
| glui | Game menu UI built on gl2d |
| imgui-docking | Debug/tool UI with docking and Font Awesome icons |
| glm | Math (vectors, matrices) |
| GLFW 3.3.2 | Windowing and input |
| GLAD | OpenGL 3.3 loader |
| raudio | Audio playback (miniaudio-based) |
| safeSave | Binary save system with automatic backups |
| stb_image/stb_truetype | Image and font loading |
| enet 1.3.17 | Networking (commented out by default — uncomment in CMakeLists.txt to enable) |

## Input System

Access via `platform::Input &input` parameter in `gameLogic()`:
- `input.isButtonHeld/Pressed/Released(platform::Button::X)` for keyboard
- `input.lMouse`, `input.rMouse` for mouse buttons
- `input.mouseX`, `input.mouseY` for mouse position
- `input.deltaTime` for frame time
- `input.typedInput` for text input characters

## Platform Utilities (`platform::` namespace)

- `writeEntireFile()` / `readEntireFile()` — binary file I/O for save data
- `setFullScreen()` / `isFullScreen()` — fullscreen toggling
- `getFrameBufferSize()` — use for `glViewport` (may differ from window size)
- `getWindowSize()` — logical window dimensions
- `showMouse()` / `getRelMousePosition()` / `setRelMousePosition()`

## Rendering Pattern

```cpp
gl2d::Renderer2D renderer;
// In initGame(): gl2d::init(); renderer.create();
// In gameLogic(): renderer.updateWindowMetrics(w, h); /* draw calls */ renderer.flush();
```

## MSVC-Specific

- AVX2 SIMD enabled via `/arch:AVX2`
- Static runtime linking (MultiThreaded / MultiThreadedDebug)
- Console window suppressed via `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`
- Unicode force-disabled
