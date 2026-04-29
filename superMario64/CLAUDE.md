# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Super Mario 64 remake in C++17/OpenGL, built on the [meemknight/cmakeSetup](https://github.com/meemknight/cmakeSetup) template. Uses GLFW for windowing, glad for OpenGL loading, gl2d for 2D overlays, Dear ImGui (docking branch) for debug UI, raudio for audio, cgltf for glTF model loading, and nlohmann/json for data files.

See SPEC.md for the full game design and IMPLEMENTATION.md for technical decisions and architecture.

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

### Game Layer Files

- `input.h/.cpp` — `GameInput` struct abstracts raw `platform::Input` into semantic actions (moveDir, moveStrength, jump, crouch, attack, cameraDelta). All gameplay code reads `GameInput`, never raw keys. Mouse camera requires right-click hold; gamepad right stick always works.
- `mario.h/.cpp` — `Mario` struct with position, velocity, facingAngle, 24-state state machine (`MarioState` enum: idle, walk, run, skid, 6 jump variants, ground pound, crouch, crawl, punch combo, kick, dive, slide kick, landing). `update()` dispatches to per-state handler functions. Tracks jump chain count/timer, jump buffer/coyote timers, punch combo step. `enterState()` centralizes on-enter logic and triggers animation clip changes. Ground detection via `GroundResult` (currently a Y=0 stub, designed to swap in real raycasts later).
- `animation.h/.cpp` — Skeletal animation system. `Skeleton` (bone hierarchy + inverse bind matrices), `AnimClip` (named clip with channels of keyframes), `AnimPose` (per-bone local transforms), `AnimState` (playback state with crossfade blending). Key functions: `sampleClip()`, `blendPoses()`, `computeSkinMatrices()`, `playClip()`, `updateAnimState()`, `evaluateAnimState()`. `MAX_BONES` = 64.
- `camera.h/.cpp` — `OrbitCamera` (game camera: follows Mario, auto-centers, player-controlled orbit) and `FlyCamera` (debug camera: WASD + mouse). Shared `getProjectionMatrix()`.
- `renderer.h/.cpp` — Static meshes (`Vertex3D`/`Mesh`/`Shader`) and skinned meshes (`SkinnedVertex`/`SkinnedMesh`/`SkinnedModel`/`SkinnedShader`). `loadGLB()` for static meshes (first mesh/first primitive). `loadSkinnedGLB()` reads skeleton, bone weights, and all animation clips from glTF. `renderSkinnedMesh()` uploads bone matrices and draws with GPU skinning. Debug grid/axis via `LineMesh`.

### Shaders

- `basic3d.vert/.frag` — MVP transform, vertex color, directional diffuse + ambient lighting.
- `skinned.vert/.frag` — Same lighting as basic3d, plus per-vertex bone matrix skinning (4 influences, `u_bones[64]` uniform array).
- `debug_line.vert/.frag` — Simple passthrough for wireframe overlays.

### Frame Loop

Physics runs on a fixed timestep (1/60s) with an accumulator in `gameLayer.cpp`. Each fixed tick: map input → `mario.update()` → `updateAnimState()`. Each variable frame: `orbitCamera.update()` → `evaluateAnimState()` (compute skin matrices) → render. Key 2 toggles fly camera as a debug override. Key 1 toggles ImGui debug UI.

## Key Compile-Time Macros

- `RESOURCES_PATH` — always `"./resources/"` (relative to executable). Used as a prefix for asset loading: `RESOURCES_PATH "myfile.png"`.
- `PRODUCTION_BUILD` / `DEVELOPLEMT_BUILD` (note: typo is intentional, kept for compatibility) — set automatically based on Debug/Release config. Controls debug console allocation on Windows and assert behavior.
- `REMOVE_IMGUI` — defined in `platformTools.h` and `glfwMain.cpp`. Set to `1` to strip ImGui entirely.

## ImGui

Uses Dear ImGui v1.92.7-docking. The `thirdparty/imgui-docking/` directory also contains extra addon headers used by the project: `imguiThemes.h`, `IconsForkAwesome.h`, `imfilebrowser.h`, and `multiPlot.h`/`multiPlot.cpp`. When upgrading ImGui, watch for API breaking changes — this project has already been migrated from older signatures (e.g. `DockSpaceOverViewport`, `EndChildFrame`, `ImGuiWindowFlags_AlwaysUseWindowPadding`).

## Conventions

- Sources are auto-discovered via `GLOB_RECURSE` on `src/*.cpp` — no need to register new files in CMake.
- All thirdparty dependencies live in `thirdparty/` with their own CMakeLists.txt and are linked statically.
- ENet (networking) is included but commented out in CMakeLists.txt — uncomment both the `add_subdirectory` and the `target_link_libraries` line to enable it.
- Save data uses raw struct serialization via `platform::readEntireFile` / `platform::writeEntireFile` into the resources directory.
- Coordinate system: Y-up, 1 unit = 1 meter. Matches Blender, glTF, and OpenGL defaults — no conversion needed.

## Gotchas

- **Blender is Z-up, engine is Y-up.** The glTF exporter converts automatically, but Blender scripts must use Z for height. A model placed at Blender `(0, 5, 0)` ends up at engine `(0, 0, -5)`, not `(0, 5, 0)`.
- **`platform::log()` takes a plain `const char*`**, not printf format args. Build strings with `std::string` and call `.c_str()`.
- **`platform::Input` keyboard buttons** are limited to A–Z, 0–9, Space, Enter, Escape, arrows, Ctrl, Tab, Shift, Alt. No function keys (F1–F12). Use number keys for debug toggles.
- **glad was generated without GL_DEBUG_OUTPUT.** The `errorReporting.cpp` is guarded by `#ifdef GL_DEBUG_OUTPUT` and compiles to no-ops.
- **`#define GLM_ENABLE_EXPERIMENTAL`** must appear before any GLM experimental headers (e.g. `glm/gtx/transform.hpp`, `glm/gtx/quaternion.hpp`). Every `.cpp` that uses GLM experimental features needs this define at the top, or MSVC will error with `#error "GLM: GLM_GTX_... is an experimental extension"`.
- **Blender 5.1 glTF export API** does not accept `export_colors` as a keyword argument (changed from earlier versions). Use `export_normals=True` and omit `export_colors`. Vertex colors export by default.

## Known Benign Warnings

- `APIENTRY` macro redefinition (glad vs Windows SDK) — harmless, can be ignored.
- CMP0115 policy warnings from `stb_image` and `stb_truetype` — these are upstream CMake dev warnings, not build issues.

## Coding Principles

- **C++ game programming best practices**
- **KISS** — simplest thing that works. No clever patterns where a plain `if` does the job. But a plain `if` that must be copy-pasted into every new feature is not simple — it's a maintenance trap.
- **YAGNI** — don't build for hypothetical needs. No abstraction layers "for later."
- **DRY** — remove real duplication, not shape-similar code. Wrong abstraction costs more than repetition.
- **Locality of change** — adding a new entity, tile, or feature should require changes in as few files as possible. Prefer data-driven dispatch (flags, vtables) over centralized type switches when the set of types is expected to grow. If a new enemy requires editing the orchestrator, the abstraction is missing.

When in doubt: for code one person owns and rarely changes, lean KISS. For interfaces many contributors touch, lean locality of change.

## Asset Pipeline

- **3D models + animations**: Blender 5.1 Python scripts in `tools/models/`. Models built procedurally with armatures and keyframed animations, exported as glTF/GLB. Character models (e.g. `mario.py`) create a bone hierarchy, skin the mesh with vertex groups, define animations as NLA tracks, and export with `export_skins=True, export_nla_strips=True`. Static models (e.g. `test_scene.py`) export geometry only. Output to `resources/models/` (characters, enemies, objects) and `resources/courses/` (level geometry). Run headless: `"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" --background --python <script>.py`
- **Sprites**: spritesheets drawn as single SVGs with all frames on a 64px grid, exported as one PNG via Inkscape (`"C:/Program Files/Inkscape/bin/inkscape.exe" player.svg -o player.png -w 256 -h 256` for a 4x4 sheet). One sheet per category (player, enemy type, tiles, items, etc.). Code indexes frames by row/column source rectangle. Store SVG and PNG in `resources/sprites/`.
- **Sounds**: Python scripts in `tools/sfx/` using numpy + scipy. Each sound is a parameterized function (waveform synthesis, envelopes, filters). Output WAV to `resources/sfx/`.
- **Music**: Python scripts in `tools/music/` using `midiutil` to generate MIDI. FluidSynth renders with a soundfont to WAV. ffmpeg converts to OGG. Output to `resources/music/`.

### External Tool Paths

- Blender: `"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"`
- Inkscape: `"C:/Program Files/Inkscape/bin/inkscape.exe"`
- FluidSynth: `D:\fluidsynth-v2.5.4-win10-x64-cpp11`
- Soundfont: `D:\GeneralUser-GS\GeneralUser-GS.sf2`
- ffmpeg: `D:\ffmpeg-8.1-essentials_build`

### Python Dependencies

numpy, scipy, midiutil — install via `pip install numpy scipy midiutil`.
