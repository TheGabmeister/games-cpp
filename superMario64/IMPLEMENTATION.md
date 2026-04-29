# Implementation Guide

High-level technical decisions and architecture for building the game described in SPEC.md.

---

## Coordinate System and Scale

Y-up everywhere (Blender, glTF, OpenGL — no conversion needed). 1 unit = 1 meter. Reference sizes: Mario ~1.5 units tall, Goomba ~0.8, coin ~0.5 diameter, standard platform ~4 wide, camera default distance ~8 behind Mario.

---

## Project Structure

The existing two-layer architecture stays. Platform layer (main loop, GLFW, input, file I/O) is untouched — all new code goes in the game layer. Start with few files, split when they grow large:

```
src/gameLayer/
├── gameLayer.cpp      # Entry points, game state machine, input mapping
├── mario.cpp          # Player movement, state machine, camera
├── world.cpp          # Level loading, collision, entities
├── renderer3d.cpp     # 3D rendering pipeline, mesh loading
└── audio.cpp          # SFX and music playback

include/gameLayer/
└── (matching headers)
```

---

## Game Loop Integration

The platform layer already calls `gameLogic(float deltaTime, ...)`. We add a top-level `GameState` enum (TITLE_SCREEN, FILE_SELECT, GAMEPLAY, PAUSE, QUIT) and switch on it each frame to route to the appropriate update/render functions.

Physics (movement, gravity, friction, collision) runs on a fixed timestep (1/60s) with an accumulator. Timers, animation, and rendering use variable `deltaTime`.

---

## Input Abstraction

Map raw `platform::Input` into a `GameActions` struct with semantic fields: move direction (vec2, normalized), move strength (0–1, analog-aware), and booleans for jump, jumpHeld, attack, crouch, crouchHeld, interact, pause, camera controls. Gamepad sticks provide analog strength; keyboard is always 1.0.

Mouse deltas for camera control are computed in the game layer by storing last frame's mouse position and subtracting — no platform layer changes needed.

---

## Rendering

- **Resolution:** 1200x900 (4:3 aspect ratio). Windowed by default.
- **3D world:** OpenGL 3.3 Core. Levels built from basic geometry. Characters and enemies are simple meshes or billboarded sprites. Flat/vertex colors with basic diffuse shading (single directional light + ambient).
- **2D overlays:** gl2d + glui for HUD and menus, rendered in orthographic projection on top of the 3D scene.

### Frame Render Order

1. **Clear** — sky color + depth buffer.
2. **Skybox** — gradient or textured fullscreen quad, depth disabled.
3. **Opaque geometry** — level and solid objects, depth testing on.
4. **Characters and enemies** — 3D meshes or billboarded sprites.
5. **Transparent geometry** — water surfaces, Vanish Cap Mario, coins, particles. Back-to-front.
6. **2D overlay** — orthographic pass for HUD/menus, depth testing off.

### View and Projection

Perspective matrix (FOV ~45°, near 0.1, far 500). View from `glm::lookAt`. Each object carries its own model matrix.

### Frustum Culling

Sphere-based per-object check against the 6 frustum planes. Level geometry can be split into chunks for coarser culling on larger courses.

### Mesh Loading

Models use glTF/GLB format (supports static meshes and skeletal animation in one format). Loaded via cgltf (header-only C library, vendored in `thirdparty/cgltf/`), parsed at load time into VBOs (position, normal, UV/color). Levels are static meshes loaded once per course. Animated characters use skeletal animation data from the glTF file.

### Model Authoring

3D models and levels are generated via headless Blender Python scripts in `tools/models/`. Scripts build meshes procedurally from primitives (cubes, cylinders, planes), assign vertex colors, set up armatures and keyframe animations, and export to glTF/GLB. Output files go to `resources/models/` (characters, enemies, objects) and `resources/courses/` (level geometry). Collision meshes are exported as separate simplified glTF files from the same scripts.

Blender path: `"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" --background --python <script>.py`

---

## Shader Approach

Start with a single basic shader (vertex color + directional light + ambient). Add separate shaders as needed when new rendering features demand them (water, transparency, particles, textures, etc.). Vertex colors only to start; add texture support in Phase 3+.

---

## Entity System

Hybrid inheritance + components. A base `Entity` class holds shared fields (position, velocity, collider, active flag) and virtual `update()`/`render()`. Each entity type is a subclass (Goomba, BobOmb, Star, etc.) with its own logic. Optional reusable components (Physics, AI, Health, Animator) are attached per-entity — the base `update()` ticks attached components, then the subclass runs its specific logic. This avoids deep inheritance hierarchies while keeping entity types readable as single classes. Components are reusable across projects.

---

## Mario State Machine

An enum of ~30 states (idle, walking, running, crouch, all jump variants, combat moves, ledge grab, freefall, swimming, carrying, knockback, dead). Each frame: read input, run state-specific update (which may transition state), apply physics, move-and-collide, update animation. The Mario struct holds position, velocity, facing angle, state, jump chain tracking, coyote/buffer frames, combo step, health, lives, active cap, and cached collision results from the current frame.

---

## Collision Detection

### Level Collision

Three separate checks each frame: ground (downward ray/sphere to find floor height and surface type), walls (horizontal sweep for obstruction and sliding), and ceiling (upward check). The level collision mesh is a triangle mesh where each triangle is tagged with a surface type and layer bits. Uniform grid for spatial partitioning — divide the level into equal-sized cells, each storing its triangles. Query only the cells near Mario.

### Entity Collision

Entities use primitive collider shapes attached as components: sphere (Goombas, coins, Bob-ombs), capsule (Mario, tall characters), or AABB (crates, blocks, triggers). Entity-vs-entity checks use primitive-vs-primitive tests. Entity-vs-level checks test the primitive against nearby level triangles from the grid.

### Collision Layers

Each collider (entity or level triangle) has two bitmasks: a `category` (what I am) and a `mask` (what I collide with). Two objects only collide if `(a.category & b.mask) && (b.category & a.mask)`. Layers:

- Bit 0: `PLAYER` — Mario.
- Bit 1: `ENEMY` — Goombas, Bob-ombs, Koopas, etc.
- Bit 2: `PLAYER_ATTACK` — Mario's punch/kick/dive hitboxes.
- Bit 3: `COLLECTIBLE` — coins, stars, 1-ups.
- Bit 4: `TERRAIN` — level geometry (floors, walls, ceilings).
- Bit 5: `TRIGGER` — warps, loading zones, event volumes.
- Bit 6: `INTANGIBLE` — Vanish Cap Mario, decorations.
- Bit 7: `PROJECTILE` — Bob-omb explosions, fire.

Examples: an enemy sets category=ENEMY, mask=PLAYER|PLAYER_ATTACK|TERRAIN. A coin sets category=COLLECTIBLE, mask=PLAYER. Vanish Cap Mario sets category=INTANGIBLE, mask=TERRAIN (walks on floors but passes through enemies).

---

## Level Loading

Each course has a visual mesh (glTF/GLB), a separate collision mesh (simplified, surface-typed triangles), and an object placement file listing all spawns with type, position, and optional fields (patrol path, star filter, linked star ID, condition). Course metadata (name, star names, sky color, music track) is stored alongside or in a master course list. Early phases can hardcode everything; file-based loading comes in Phase 3+.

Object placement data parsed with nlohmann/json (vendored in `thirdparty/nlohmann_json/`).

---

## Camera

Orbit camera that tracks Mario at chest height. Player controls orbit angle and pitch (clamped 10°–70°) via right stick or mouse. When no manual input, the camera auto-centers behind Mario's movement direction. A raycast from the target point to the desired camera position detects level geometry and pushes the camera forward to avoid clipping. Position and target are smoothly interpolated each frame. Multiple modes: follow (default), boss arena (fixed position), underwater, cannon aim, first person, cutscene.

---

## Audio Manager

Thin wrapper around raudio. Music: one track playing at a time, crossfade on area transitions (~1s). SFX: preloaded WAV array indexed by enum, fire-and-forget playback. OGG for music (smaller files), WAV for SFX (no decode latency). Underwater sections apply a low-pass filter or swap to a muffled track variant. Cap expiry warning beeps during the last 5 seconds. Footstep SFX frequency scales with movement speed.

### SFX Generation

SFX are generated via Python scripts in `tools/sfx/` using numpy (waveform synthesis) and scipy (WAV export, filters). Each sound type is a parameterized function — pitch, duration, envelope, layering — so sounds are reproducible and tunable. Output WAV files go to `resources/sfx/`.

### Music Generation

Python scripts in `tools/music/` using `midiutil` to generate MIDI. FluidSynth renders MIDI to WAV using the GeneralUser-GS soundfont. ffmpeg converts WAV to OGG. Output OGG files go to `resources/music/`.

Tool paths:
- FluidSynth: `D:\fluidsynth-v2.5.4-win10-x64-cpp11`
- Soundfont: `D:\GeneralUser-GS\GeneralUser-GS.sf2`
- ffmpeg: `D:\ffmpeg-8.1-essentials_build`

---

## Save System

Raw struct serialization via the existing `platform::writeEntireFile` / `platform::readEntireFile`. The save struct has a magic number and version field for corruption/versioning detection. Tracks: stars collected per course, total stars, keys obtained, cap switches pressed, lives. Four save file slots, saved to `resources/save_N.dat`.

---

## Debug Tooling

ImGui docking windows, debug builds only (`DEVELOPLEMT_BUILD`). Game viewport stays in the center; debug windows dock to edges. F1 toggles all debug UI on/off.

- **FPS / frame time** — always-visible top bar.
- **Collision visualizer** — wireframe overlay of level collision mesh, entity colliders, and contact points.
- **Mario state inspector** — live view of state, velocity, position, onGround, surface type, jump chain, coyote frames, active cap.
- **Entity inspector** — list of active entities with type, position, AI state, health. Select to highlight.
- **Free camera** — detach from Mario and fly around the level.
- **Grid / axis overlay** — world origin and ground grid for verifying positions and scale.
- **Collision layer viewer** — toggle visibility of individual collision layers.

---

## Build Changes

- Window size: change default from 500x500 to 1200x900 in `glfwMain.cpp`.
- New thirdparty headers (cgltf, nlohmann/json) added to `thirdparty/`.
- Shader files go in `resources/shaders/` (copied by the existing post-build step).

---

## Implementation Priority

For incremental development, implement in this order:

1. **Phase 1 — Core Movement:** Mario controller with full movement set on a flat test level. Camera system.
2. **Phase 2 — Physics:** Slopes, gravity, ledge grabs, wall jumps, swimming. Health system.
3. **Phase 3 — First Course:** Bob-omb Battlefield with Goombas, Bob-ombs, King Bob-omb boss, 7 stars.
4. **Phase 4 — Hub World:** Peach's Castle exterior + first floor interior. Star doors, course entry via paintings.
5. **Phase 5 — Bowser Fight:** Bowser in the Dark World level + boss arena.
6. **Phase 6 — Additional Courses:** Build out remaining courses incrementally.
7. **Phase 7 — Cap Power-Ups:** Wing, Metal, Vanish caps and their secret stages.
8. **Phase 8 — Polish:** Save system, lives/game-over flow, remaining secret stars, title screen.
