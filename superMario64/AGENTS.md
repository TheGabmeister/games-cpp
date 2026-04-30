# AGENTS.md

Guidance for coding agents working in this repository.

## Project Overview

This is a C++17/OpenGL game project built from the `meemknight/cmakeSetup`
template. It uses GLFW for windowing, glad for OpenGL loading, gl2d/glui for
2D overlays and menus, Dear ImGui docking for debug UI, and raudio for audio.

The goal is an SM64-inspired 3D platformer prototype. Treat `SPEC.md` as the
content/design target and `IMPLEMENTATION.md` as the source of truth for
technical decisions.

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

## Game-Specific Decisions

- Keep high-level technical choices aligned with `IMPLEMENTATION.md`. If a task
  requires changing one of those choices, update the document in the same
  change.
- Use a Y-up coordinate system everywhere: Blender, glTF, OpenGL, gameplay, and
  collision. Scale is 1 unit = 1 meter; Mario is roughly 1.5 units tall.
- Prefer a vertical-slice workflow. Prove core movement, camera, collision, and
  one simple playable test level before expanding content volume.
- Keep all runtime gameplay code in `src/gameLayer/` and `include/gameLayer/`
  unless a platform-layer change is truly required.
- All assets should be original, generated, or royalty-free. Do not add ripped
  Nintendo assets, audio, models, textures, or proprietary data.

## Current Development Focus

- Current milestone: Phase 4 -- World Collision & Physics in
  `IMPLEMENTATION.md`.
- Phases 1-3 are treated as functionally complete unless the task is explicitly
  about fixing regressions in rendering, movement, camera, combat, or animation.
- Prioritize separate collision GLB loading, triangle-grid queries, ground/wall/
  ceiling collision, slope classification, step-up handling, and surface-type
  behavior before expanding entities, courses, HUD, audio, or menus.
- Keep coyote time and jump buffering behavior compatible with the movement
  state machine while moving grounded checks from the flat Y=0 plane to level
  geometry.
- When adding collision test content, prefer generated or simplified assets under
  `tools/` and `resources/courses/` so the visual level and collision mesh can
  stay reproducible.

## Runtime Systems

- Route the game through a top-level game state machine: title/file select,
  gameplay, pause, and quit.
- Run gameplay physics on a fixed 1/60s timestep with an accumulator. Rendering,
  animation interpolation, and non-simulation timers may use variable
  `deltaTime`.
- Map `platform::Input` to semantic game actions before gameplay logic. Do not
  scatter raw key/button checks through movement, entity, or UI systems.
- Use OpenGL 3.3-era rendering for the 3D world and draw gl2d/glui HUD or menus
  as a final orthographic overlay.
- Use glTF/GLB for models and course geometry through `cgltf`. Vertex colors are
  the starting art path; texture support can be added when the relevant phase
  needs it.
- Generate models, levels, collision meshes, SFX, and music through scripts
  under `tools/` when possible so assets are reproducible.

## Collision And Levels

- Level collision is triangle-mesh based. Triangles carry surface type and
  collision layer bits, and are partitioned with a uniform grid for queries.
- Entity collision uses simple primitives: capsule, sphere, or AABB. Keep
  primitive-vs-primitive and primitive-vs-level checks straightforward before
  adding special cases.
- Use category/mask bitmasks for collision filtering. Preserve the layer intent
  from `IMPLEMENTATION.md`: player, enemy, player attack, collectible, terrain,
  trigger, intangible, and projectile.
- Each course should eventually have a visual glTF/GLB, a simplified collision
  mesh, object placement data, and course metadata. Hardcoding is acceptable in
  early phases, but do not make hardcoded paths or assumptions hard to migrate.

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

## Coding Principles

- **C++ game programming best practices**
- **KISS** — simplest thing that works. No clever patterns where a plain `if` does the job. But a plain `if` that must be copy-pasted into every new feature is not simple — it's a maintenance trap.
- **YAGNI** — don't build for hypothetical needs. No abstraction layers "for later."
- **DRY** — remove real duplication, not shape-similar code. Wrong abstraction costs more than repetition.
- **Locality of change** — adding a new entity, tile, or feature should require changes in as few files as possible. Prefer data-driven dispatch (flags, vtables) over centralized type switches when the set of types is expected to grow. If a new enemy requires editing the orchestrator, the abstraction is missing.

When in doubt: for code one person owns and rarely changes, lean KISS. For interfaces many contributors touch, lean locality of change.

## Debug Tooling

- ImGui debug tooling is for debug/development builds and should be guarded by
  `DEVELOPLEMT_BUILD` where practical.
- Useful debug views include frame timing, Mario state, entity inspection,
  collision visualization, collision layer toggles, grid/axis overlays, and a
  free camera.
- Do not let debug UI become required for normal gameplay flow.

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
