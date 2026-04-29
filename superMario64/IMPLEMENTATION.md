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
├── renderer.cpp     # 3D rendering pipeline, mesh loading
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

For incremental development, implement in this order. Each phase produces a testable build.

### Foundation (Phases 1–2)

1. **Phase 1 — Rendering Foundation & Debug Tools:** 3D rendering pipeline (vertex-color shader, perspective projection, view matrix). glTF/GLB mesh loading via cgltf. Flat test level with placeholder geometry. Window set to 1200×900. ImGui debug overlay: FPS counter, grid/axis overlay, free-fly camera.
2. **Phase 2 — Mario Ground Movement & Camera:** Input abstraction (`GameInput` struct). Mario model (Blender-generated). Walk/run/idle/skid on flat ground with analog speed. Single jump with variable height. Gravity + ground detection (downward raycast). Orbit camera with player-controlled rotation and pitch, smooth follow, auto-center.

### Core Mechanics (Phases 3–7)

3. **Phase 3 — Full Movement Set & Animation:** All jump variants (double, triple, long, backflip, side somersault). Crouch, crawl. Punch combo, kick, dive, slide kick. Jump buffering + coyote time. Skeletal animation system driven by the state machine (~30 states). Mario animations from Blender.
4. **Phase 4 — World Collision & Physics:** Collision mesh loading (separate simplified glTF per level). Uniform-grid spatial partition. Wall detection + sliding. Ceiling detection + head bonk. Slope behavior (walkable < 30°, steep 30–50°, wall > 50°). Surface types (normal, ice). Step-up tolerance for small ledges.
5. **Phase 5 — Advanced Movement & Interactions:** Wall jump. Ledge grab + climb. Pole/tree climbing. Object carrying + throwing. Platform types: moving (Mario inherits velocity), falling (delay + respawn), tilting, one-way, breakable (ground pound).
6. **Phase 6 — Health, Damage & HUD:** 8-segment power meter. Damage from contact/hazards (1–3 segments). Knockback impulse + invincibility frames. Fall damage threshold. Void/pit instant death. Coins (yellow/red/blue) as collectibles + healing. Spinning Heart. HUD overlay via gl2d: health, coin counter, star counter, lives.
7. **Phase 7 — Swimming & Water:** Surface swimming (A to paddle). Underwater free movement (A for burst, stick for pitch/yaw). Reduced underwater gravity. Air meter (health meter drains ~1 segment per 4s). Air replenished by coins or surfacing. Drowning death. Water plane rendering (transparent quad). Camera: looser underwater follow, expanded pitch range.

### First Playable Loop (Phases 8–12)

8. **Phase 8 — Entity System & Core Enemies:** Base `Entity` class with optional reusable components (Physics, AI, Health, Animator). Spawn/despawn by distance. AI behaviors: patrol (back-and-forth or loop), detection radius, chase (direct movement toward Mario), terrain-edge respect. Implement: Goomba (patrol → charge → squished), Bob-omb (patrol → chase → explode; grabbable from behind), Koopa Troopa (patrol → flee; shell drop), Boo (approach when Mario's back is turned, intangible when faced). Generic defeat → coin drop.
9. **Phase 9 — Audio System:** Thin raudio wrapper: one music stream (OGG, crossfade on transitions), SFX array (WAV, fire-and-forget). Generate initial SFX via Python scripts (movement: footsteps, jumps, land; combat: punch, kick, hit; collectibles: coin, star appear). Generate first music tracks via Python/MIDI pipeline (overworld, boss fight). Hook SFX into existing movement and combat code. Footstep frequency scales with speed.
10. **Phase 10 — Bob-omb Battlefield (First Playable Course):** Level geometry + collision mesh from Blender script. Object placement via JSON. All 7 stars: King Bob-omb boss (grab from behind, throw 3×, must stay on summit), red coins, 100-coin star, Chain Chomp (ground-pound post to free), cannon (Bob-omb Buddy NPC unlocks, aim-and-launch UI), remaining mission stars. Star select screen on course entry. Course exit on star collection.
11. **Phase 11 — Hub World: Peach's Castle:** Castle exterior (grounds, moat, courtyard) + interior (first floor, basement structure, second floor locked). Star doors (gated by star count: 1, 3, 8, 12, 30, 50, 70). Painting portals → star select → course load. Toad NPCs (dialog on interact). Regular doors. Basement accessible after Phase 12. Second floor accessible after Phase 14.
12. **Phase 12 — Bowser in the Dark World:** Linear obstacle-course level (platforms, flames, moving hazards). Bowser boss arena: circular platform with 5 spiked bombs. Grab-spin-throw mechanic (stick rotation builds angular momentum, release direction matters). Bowser AI: fire breath (arc toward Mario), charge (sidestep window). 1 hit to defeat. Reward: basement key.

### Course Expansion (Phases 13–18)

13. **Phase 13 — Courses 2–5:** Whomp's Fortress (Whomps, Whomp King boss, Thwomps, Piranha Plants, poles). Jolly Roger Bay (underwater exploration, Unagi eel, sunken ship). Cool, Cool Mountain (ice surface physics, slide course, penguin race/escort). Big Boo's Haunt (Boo mechanics, Big Boo boss, Mr. I). Each course: Blender geometry, collision mesh, 7 stars, course-specific enemies.
14. **Phase 14 — Bowser in the Fire Sea:** Second Bowser level (lava hazards, tilting platforms). Bowser adds ground-pound shockwave (expanding ring, jump to dodge). Faster movement. Arena tilts after taking damage. 1 hit. Reward: second floor key. Unlocks castle second floor and upper courses.
15. **Phase 15 — Courses 6–9:** Hazy Maze Cave (underground, Dorrie, toxic maze). Lethal Lava Land (lava surface type, Bully/Big Bully boss, rolling log). Shifting Sand Land (desert, pyramid interior, Eyerok boss, Pokey, quicksand surfaces). Dire, Dire Docks (underwater dock, submarine, poles). Course-specific enemies: Bully, Eyerok, Pokey, Monty Mole, Dorrie. Heave-Ho.
16. **Phase 16 — Cap Power-Ups & Secret Stages:** Wing Cap (flight on triple jump/cannon: stick up = dive, stick down = climb; 60s duration). Metal Cap (invulnerable, heavy, walk underwater with infinite air; 20s). Vanish Cap (semi-transparent, pass through specific walls/grates/enemies; 20s). Three cap switch stages (accessed from castle hub). Cap blocks in courses (only dispense after switch pressed). Combined caps where applicable.
17. **Phase 17 — Courses 10–15:** Snowman's Land (giant snowman, wind zone surface type, ice). Wet-Dry World (water level manipulation mechanic). Tall, Tall Mountain (vertical course, Fly Guy, Ukiki). Tiny-Huge Island (scale-switching between tiny and huge versions). Tick Tock Clock (moving gears/platforms synced to clock time on entry). Rainbow Ride (magic carpet ride, floating platforms). Enemies: Fly Guy, Amp, Lakitu, Spiny, Wiggler boss.
18. **Phase 18 — Bowser in the Sky:** Final Bowser level (longest obstacle course, all hazard types). Bowser uses all attacks: fire breath, charge, ground-pound shockwave, fire rain (fireballs from sky with shadow indicators). 3 hits to defeat. After hit 2, arena outer edge crumbles to star shape (fewer bombs, smaller arena). Third throw needs more spin momentum. Reward: Grand Star → ending sequence.

### Game Flow & Polish (Phases 19–20)

19. **Phase 19 — Menus, Save System & Game Flow:** Title screen (castle background, "Press Start", idle demo optional). File select (4 slots, star count display, new/continue/copy/delete). Pause menu (continue/exit course). Game Over screen (continue → file select, quit → title). Save system: raw struct serialization (`save_N.dat`), magic number + version field, saves on star collection + course exit. Lives system: start at 4, 1-Up mushrooms, extra lives from coins (50 + 100), Game Over at 0.
20. **Phase 20 — Polish & Completion:** Secret courses: Princess's Secret Slide, The Secret Aquarium, Wing Mario Over the Rainbow, Cavern of the Metal Cap, Vanish Cap Under the Moat. Castle secret stars: MIPS the rabbit (2 stars at different star counts), Toad gifts, hidden slide stars. Ending sequence: Bowser defeated cutscene, Peach rescued, cake scene. 120-star bonus: Yoshi on castle rooftop (99 lives + message). Remaining SFX/music/jingles. Final tuning pass on movement parameters.

---

## Phase 1 Detail — Rendering Foundation & Debug Tools

### Goal

Replace the 2D rectangle demo with a 3D test scene. 1200×900 window, vertex-colored geometry with directional lighting, free-fly debug camera, ImGui overlays. This scene becomes the playground for Phase 2.

### Files

**Create:**
- `include/gameLayer/renderer.h` + `src/gameLayer/renderer.cpp` — Structs: `Vertex3D` (position, normal, color), `Mesh` (VAO/VBO/EBO), `Shader` (program + cached uniform locations), `FlyCamera` (position, yaw, pitch). Functions: shader loading, mesh creation/destruction, `loadGLB()` via cgltf, debug grid/axis generation, render calls.
- `resources/shaders/basic3d.vert` + `basic3d.frag` — Vertex color + directional light + ambient. Vertex inputs: position (loc 0), normal (loc 1), color (loc 2). Uniforms: MVP, model matrix, light direction, ambient strength.
- `resources/shaders/debug_line.vert` + `debug_line.frag` — Unlit colored lines. Vertex inputs: position (loc 0), color (loc 1). Uniform: VP matrix.
- `tools/models/test_scene.py` — Blender script: vertex-colored ramp + cylinder + stepped platform → `resources/models/test_scene.glb`. Validates the Blender→GLB→engine pipeline.

**Modify:**
- `glfwMain.cpp` — Window size 500×500 → 1200×900, title `"geam"` → `"Super Mario 64"`.
- `gameLayer.cpp` — Replace rectangle demo. Keep gl2d init for future HUD. Add: load shaders, build hardcoded test geometry, load test GLB, enable depth test + back-face culling, render loop, ImGui debug windows.

### Key Decisions

- **Hardcoded test geometry first, then glTF.** Ground plane (50×50, dark green) + 5 colored cubes at known positions (white at origin, red/dark-red on ±X, blue/dark-blue on ±Z). This verifies the pipeline without any file dependency. The Blender GLB is loaded alongside it as a second verification.
- **Two shaders.** Lit shader for solid geometry, unlit line shader for debug overlays (grid + axes). Don't try to share one shader for both.
- **FlyCamera** controls: WASD + mouse, toggled by F2. When active, cursor is hidden. Pitch clamped ±89°. Speed adjustable via ImGui slider.
- **F1 toggles all debug UI** (ImGui windows, grid, axes).
- **Existing 2D shaders** (`resources/vertex.vert`, `resources/fragment.frag`) are unused by the game — gl2d has its own. Leave them alone.
- **glTF loading** — Phase 1 only loads the first mesh/first primitive from a GLB. Multi-mesh comes later.

### Steps (in order)

1. Window size + title change in `glfwMain.cpp`.
2. Write the 4 shader files.
3. `renderer.h/.cpp` — shader compile/link, mesh VAO/VBO/EBO creation, render functions.
4. Hardcoded test geometry (ground plane + 5 cubes). Wire into `gameLayer.cpp` with depth test + culling. Verify rendering.
5. FlyCamera (WASD + mouse). Verify navigation.
6. `loadGLB()` via cgltf. Load the Blender test scene. Verify it renders alongside hardcoded geometry.
7. Debug grid + axis overlays with the line shader.
8. ImGui debug windows (FPS, camera info, toggles).

### Test Checklist

**Rendering:**
- [x] Window opens at 1200×900, sky blue background, title "Super Mario 64"
- [x] Ground plane visible at floor level, all 5 cubes at correct positions with correct colors
- [x] Directional lighting visible (lit vs shadowed faces), ambient prevents pure-black faces
- [x] Depth testing works (near objects occlude far), no inside-faces visible (back-face culling)

**glTF:**
- [x] `blender --background --python tools/models/test_scene.py` runs without errors
- [x] Test model renders with correct vertex colors and geometry, lighting looks right

**Camera:**
- [x] F2 toggles fly camera on/off (cursor hides/shows)
- [x] WASD moves relative to view direction, mouse rotates view, no gimbal lock at steep pitch
- [x] Scene visible on startup without needing to move

**Debug overlays:**
- [x] Grid visible on ground, axis lines at origin (red=X, green=Y, blue=Z)
- [x] F1 toggles all debug UI, FPS counter works, camera info updates live

**Edge cases:**
- [x] Window resize updates viewport and aspect ratio correctly
- [x] Alt-tab and return doesn't break rendering
- [x] gl2d initializes without errors (even though unused)

---

## Phase 2 Detail — Mario Ground Movement & Camera

### Goal

A controllable Mario on the test level. Walk/run with analog speed, skid on direction reversal, single jump with variable height, gravity. Orbit camera that follows Mario. Input abstracted so gameplay code never reads raw keys. Physics on a fixed timestep. By the end, the game feels like a basic 3D platformer on flat ground.

### Files

**Create:**
- `include/gameLayer/input.h` — `GameInput` struct: `vec2 moveDir` (normalized), `float moveStrength` (0–1), booleans `jump`, `jumpHeld`, `crouch`, `crouchHeld`, `attack`, `interact`, `pause`, `vec2 cameraDir` (right stick / mouse delta), `bool cameraZoom`. Function: `GameInput mapInput(platform::Input&, prevMouseX, prevMouseY)` that reads keyboard/gamepad and fills the struct. Gamepad sticks give analog strength; keyboard is always 1.0.
- `include/gameLayer/camera.h` + `src/gameLayer/camera.cpp` — `OrbitCamera` (game camera): follows a target point, player-controlled yaw/pitch, smooth lerp, auto-center behind Mario's movement direction. Move `FlyCamera` here from renderer. Both cameras produce a view matrix; `gameLayer.cpp` picks which one is active.
- `include/gameLayer/mario.h` + `src/gameLayer/mario.cpp` — `Mario` struct: position, velocity, facingAngle, state enum, onGround, jumpHeld tracking. States for Phase 2: `IDLE`, `WALKING`, `RUNNING`, `SKIDDING`, `JUMP_ASCEND`, `FREEFALL`, `LANDING`. Update function takes `GameInput` + `float fixedDt`, returns nothing. Handles movement, state transitions, gravity, ground detection.
- `tools/models/mario.py` — Blender script: simple vertex-colored humanoid (~1.5 units tall). Red cap + torso, blue overalls/legs, skin-colored face/hands. No armature yet (animation is Phase 3). Export to `resources/models/mario.glb`.

**Modify:**
- `renderer.h/.cpp` — Remove `FlyCamera` (moved to camera.h). Keep rendering API unchanged.
- `gameLayer.cpp` — Fixed timestep loop (1/60s accumulator). Each fixed tick: map input → `mario.update()` → `orbitCamera.update()`. Each frame: render Mario mesh at Mario's position + facing rotation, render test level, debug UI.

### Key Decisions

- **Fixed timestep with accumulator.** Physics at 60 Hz. Accumulate `deltaTime`, step in `FIXED_DT = 1/60s` increments. No interpolation for now (vsync at 60 fps matches, and it's simpler). Add interpolation later if jitter is visible.
- **Ground detection is trivial for Phase 2.** No collision mesh yet (Phase 4). Just check `if (mario.position.y <= 0) → onGround, groundY = 0`. Design the interface so it's easy to swap in real raycasts later: a `GroundResult { bool hit; float groundY; vec3 normal; }` returned by a function we replace in Phase 4.
- **Orbit camera, not fixed camera.** Player controls horizontal orbit (right stick X / mouse X) and pitch (right stick Y / mouse Y, clamped 10°–70°). Camera sits at `target + spherical offset(yaw, pitch, distance)`. Target = Mario position + `vec3(0, 1.0, 0)` (chest height). Distance default ~8 units. Smooth lerp toward desired position (~8% per frame). Auto-center: when no camera input for ~0.5s, slowly slerp yaw toward Mario's facing direction (~2% per frame).
- **Mario faces movement direction.** `facingAngle` slerps toward the input direction. At high speed, reversing input triggers SKIDDING (brief deceleration) before turning.
- **Variable jump height.** Jump applies initial velocity (~22 units/s up). While `jumpHeld` is true and Mario is ascending, gravity is reduced to ~60%. Releasing the button restores full gravity immediately. This gives short hops vs full jumps from the same button.
- **No animation yet.** Mario model is rendered as a static mesh rotated to face `facingAngle`. Phase 3 adds the animation system and all jump/combat states.
- **Mouse delta for camera** computed in `mapInput()` by comparing current vs previous mouse position. Store previous position in `gameLayer.cpp`.

### Steps (in order)

1. `input.h` — `GameInput` struct and `mapInput()` function. Wire into `gameLayer.cpp`, verify values with ImGui.
2. `camera.h/.cpp` — Move `FlyCamera` from renderer. Add `OrbitCamera`. Verify orbit camera follows a fixed point, player can rotate it.
3. `tools/models/mario.py` — Generate Mario placeholder model. Export GLB, load in engine, render at origin.
4. `mario.h/.cpp` — Mario struct with position + velocity. Flat ground movement: acceleration, deceleration, max walk/run speed scaled by `moveStrength`. Facing angle. Render Mario mesh at his position.
5. Add gravity + jump. `JUMP_ASCEND` state with reduced gravity while held, `FREEFALL` when descending or button released. Land when `position.y <= 0`.
6. Skid state — when input direction opposes velocity at high speed, brief skid before turning.
7. Connect orbit camera to Mario's position. Auto-center behind facing direction.
8. ImGui Mario inspector: state name, position, velocity, onGround, facingAngle.

### Test Checklist

**Input:**
- [x] ImGui shows `GameInput` values updating correctly for keyboard (WASD → direction, space → jump)
- [x] Gamepad left stick controls move direction + strength (if gamepad available)
- [x] Camera rotates with mouse movement / right stick

**Movement:**
- [x] Mario walks when stick/keys are held, stops when released
- [x] Speed scales with input strength (if analog; keyboard is always full speed)
- [x] Mario faces the direction he's moving, turning is smooth (not instant snapping)
- [x] Reversing direction at speed triggers a visible skid before Mario turns around
- [x] Mario stays on the ground plane (doesn't sink or float)

**Jump:**
- [x] Pressing jump makes Mario leave the ground with upward velocity
- [x] Holding jump results in a higher arc than tapping jump
- [x] Mario falls back down and lands on the ground plane
- [x] Can't jump while already airborne (single jump only in Phase 2)
- [x] Horizontal momentum carries into the jump (running jump goes further than standing jump)

**Camera:**
- [x] Camera follows Mario as he moves, keeping him centered
- [x] Player can orbit the camera horizontally (full 360°) and adjust pitch (clamped)
- [x] Camera auto-centers behind Mario when not being manually controlled
- [x] Camera transition is smooth (no snapping or jitter)
- [x] Fly camera (key 2) still works as a debug override

**Edge cases:**
- [x] Mario can't walk off into infinity — stays in the scene (or at least doesn't crash)
- [x] Rapid jump spam doesn't break state machine
- [x] Alt-tab during a jump doesn't cause Mario to fly off
- [x] FPS counter still shows ~60 FPS with Mario moving
