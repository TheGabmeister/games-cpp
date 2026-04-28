# Implementation Guide

High-level technical decisions and architecture for building the game described in SPEC.md.

---

## Project Structure

The existing two-layer architecture stays. Platform layer (main loop, GLFW, input, file I/O) is untouched — all new code goes in the game layer.

```
src/gameLayer/
├── gameLayer.cpp          # Entry points: initGame, gameLogic, closeGame
├── player/
│   ├── mario.cpp          # Mario state machine, movement, physics
│   └── camera.cpp         # Third-person camera
├── world/
│   ├── level.cpp          # Level loading, course management
│   ├── collision.cpp      # Collision detection and response
│   └── physics.cpp        # Gravity, slopes, water, surfaces
├── entities/
│   ├── entity.cpp         # Base entity
│   ├── enemies.cpp        # Enemy AI and behaviors
│   └── objects.cpp        # Coins, stars, boxes, interactables
├── rendering/
│   ├── renderer3d.cpp     # 3D rendering pipeline, shaders
│   ├── meshLoader.cpp     # OBJ loading, VBO management
│   └── particles.cpp      # Particle system
├── ui/
│   ├── hud.cpp            # In-game HUD (health, coins, stars)
│   └── menus.cpp          # Title, file select, star select, pause
├── audio/
│   └── audioManager.cpp   # SFX and music playback via raudio
└── save/
    └── saveData.cpp       # Save/load game state

include/gameLayer/
└── (matching headers)
```

**Decision needed:** This is a flat directory approach. An alternative is fewer files with more code per file during early phases, splitting later. Which do you prefer?

---

## Game Loop Integration

The platform layer already calls `gameLogic(float deltaTime, ...)`. We add a top-level `GameState` enum (TITLE_SCREEN, FILE_SELECT, GAMEPLAY, PAUSE, QUIT) and switch on it each frame to route to the appropriate update/render functions.

**Decision needed:** Fixed timestep vs variable? For a platformer with precise physics, a fixed timestep (1/60s) with an accumulator and render interpolation is more reliable than raw `deltaTime`. Fixed timestep is the recommendation — your call.

---

## Input Abstraction

Map raw `platform::Input` into a `GameActions` struct with semantic fields: move direction (vec2, normalized), move strength (0–1, analog-aware), and booleans for jump, jumpHeld, attack, crouch, crouchHeld, interact, pause, camera controls. Gamepad sticks provide analog strength; keyboard is always 1.0.

**Decision needed:** The platform layer tracks mouse position but not deltas. Should we add delta tracking to the platform layer, or compute deltas ourselves from frame-to-frame position? Recommendation: compute in game layer to avoid modifying platform code.

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

Models loaded from OBJ, parsed at load time into VBOs (position, normal, UV/color). Levels are static meshes loaded once per course. Animated characters either swap pre-made meshes per frame or use simple bone-based vertex animation.

**Decision needed:** Model loading library:
- **tinyobjloader** — header-only, widely used, handles OBJ well.
- **Custom minimal OBJ parser** — fewer dependencies, we only need positions/normals/colors.
- **Custom binary format** — fastest loading, but needs an export pipeline.

Recommendation: tinyobjloader. We can add a binary cache later.

---

## Shader Approach

A single "basic" shader handles most rendering: vertex inputs are position, normal, and color; uniforms are model/view/projection matrices plus light direction and ambient color. Output is simple directional diffuse lighting. Add shader variants later as needed.

**Decision needed:** Should we support textures from day one, or start with vertex colors only? Vertex colors are simpler and match the low-poly goal. Textures add richness but complicate the model pipeline.

---

## Entity System

No full ECS. A flat `Entity` struct with a type enum, common fields (position, velocity, collider, AI state, model pointer), and a union for type-specific data (star ID, fuse timer, shell state, boss hit count). Update and render are dispatched by type.

**Decision needed:** The union approach is simple but limited. Alternatives:
- **Separate structs per enemy type** inheriting from a base — more type-safe, more boilerplate.
- **Component bags** (not full ECS, just optional components) — flexible, more indirection.
- **Keep the union** and extend as needed — fast, simple, good enough for ~20 types.

Recommendation: union/flat struct for this project size.

---

## Mario State Machine

An enum of ~30 states (idle, walking, running, crouch, all jump variants, combat moves, ledge grab, freefall, swimming, carrying, knockback, dead). Each frame: read input, run state-specific update (which may transition state), apply physics, move-and-collide, update animation. The Mario struct holds position, velocity, facing angle, state, jump chain tracking, coyote/buffer frames, combo step, health, lives, active cap, and cached collision results from the current frame.

---

## Collision Detection

Three separate checks each frame: ground (downward ray/sphere to find floor height and surface type), walls (horizontal sweep for obstruction and sliding), and ceiling (upward check). The collision mesh is a list of triangles, each tagged with a surface type and layer bits. A spatial acceleration structure keeps queries fast.

**Decision needed:** Spatial partitioning:
- **Uniform grid** — simplest, fast for evenly distributed geometry, wastes memory on sparse levels.
- **BVH** — best general-purpose, more complex to build.
- **Octree** — good middle ground.

Recommendation: uniform grid to start, switch to BVH if perf is an issue.

---

## Level Loading

Each course has a visual mesh (OBJ), a separate collision mesh (simplified, surface-typed triangles), and an object placement file listing all spawns with type, position, and optional fields (patrol path, star filter, linked star ID, condition). Course metadata (name, star names, sky color, music track) is stored alongside or in a master course list. Early phases can hardcode everything; file-based loading comes in Phase 3+.

**Decision needed:** For JSON parsing (object placement data):
- **nlohmann/json** — header-only, easy API, ~20k lines.
- **rapidjson** — faster, harder API.
- **Custom minimal parser** — we only need flat object arrays.
- **Skip JSON** — hardcoded structs or a simpler text format.

Recommendation: nlohmann/json, vendored as a single header.

---

## Camera

Orbit camera that tracks Mario at chest height. Player controls orbit angle and pitch (clamped 10°–70°) via right stick or mouse. When no manual input, the camera auto-centers behind Mario's movement direction. A raycast from the target point to the desired camera position detects level geometry and pushes the camera forward to avoid clipping. Position and target are smoothly interpolated each frame. Multiple modes: follow (default), boss arena (fixed position), underwater, cannon aim, first person, cutscene.

---

## Audio Manager

Thin wrapper around raudio. Music: one track playing at a time, crossfade on area transitions (~1s). SFX: preloaded WAV array indexed by enum, fire-and-forget playback. OGG for music (smaller files), WAV for SFX (no decode latency). Underwater sections apply a low-pass filter or swap to a muffled track variant. Cap expiry warning beeps during the last 5 seconds. Footstep SFX frequency scales with movement speed.

---

## Save System

Raw struct serialization via the existing `platform::writeEntireFile` / `platform::readEntireFile`. The save struct has a magic number and version field for corruption/versioning detection. Tracks: stars collected per course, total stars, keys obtained, cap switches pressed, lives. Four save file slots, saved to `resources/save_N.dat`.

---

## Build Changes

- Window size: change default from 500x500 to 1200x900 in `glfwMain.cpp`.
- New thirdparty headers (tinyobjloader, nlohmann/json) added to `thirdparty/`.
- Shader files go in `resources/shaders/` (copied by the existing post-build step).

---

## Open Decisions Summary

| # | Decision | Options | Recommendation |
|---|----------|---------|----------------|
| 1 | File structure | Many small files vs fewer larger files | Start with fewer files, split when they get large |
| 2 | Timestep | Variable (`deltaTime`) vs fixed (1/60s accumulator) | Fixed timestep — critical for consistent platformer physics |
| 3 | Mouse delta | Add to platform layer vs compute in game layer | Compute in game layer (avoid modifying platform code) |
| 4 | Model loading | tinyobjloader vs custom OBJ parser vs custom binary | tinyobjloader (header-only, reliable) |
| 5 | Textures | Vertex colors only vs textures from day one | Start vertex colors, add textures in Phase 3+ |
| 6 | Entity system | Union struct vs inheritance vs components | Union struct — simple, fast, good enough for ~20 types |
| 7 | Spatial partitioning | Uniform grid vs BVH vs octree | Uniform grid to start, BVH if needed |
| 8 | JSON library | nlohmann/json vs rapidjson vs custom parser | nlohmann/json (header-only, easiest API) |
| 9 | Shaders | Single uber-shader vs per-material shaders | Single basic shader to start, add variants as needed |

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
