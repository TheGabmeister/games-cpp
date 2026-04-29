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

For incremental development, implement in this order. Each phase produces a testable build. The checklist under each phase lists manual tests to verify before moving on.

### Foundation (Phases 1–2)

#### Phase 1 — Rendering Foundation & Debug Tools

3D rendering pipeline (vertex-color shader, perspective projection, view matrix). glTF/GLB mesh loading via cgltf. Flat test level with placeholder geometry. Window set to 1200×900. ImGui debug overlay: FPS counter, grid/axis overlay, free-fly camera.

- [x] Window opens at 1200×900. Sky-blue background renders.
- [x] Ground plane and colored cubes visible in the scene.
- [x] Test scene geometry (ramp, cylinder, stairs) loads and renders from GLB.
- [x] Grid overlay and axis gizmo visible (red=X, green=Y, blue=Z).
- [x] Press 1 to toggle ImGui debug panel on/off.
- [x] Press 2 to enter fly camera. WASD + mouse moves freely through the scene.
- [x] Press 2 again to return to the default camera.
- [x] FPS counter in debug panel shows a reasonable value and updates each frame.

#### Phase 2 — Mario Ground Movement & Camera

Input abstraction (`GameInput` struct). Mario model (Blender-generated). Walk/run/idle/skid on flat ground with analog speed. Single jump with variable height. Gravity + ground detection (downward raycast). Orbit camera with player-controlled rotation and pitch, smooth follow, auto-center.

- [x] Mario model visible at the origin, standing on the ground plane.
- [x] WASD moves Mario relative to the camera direction. W = forward from camera's perspective.
- [x] Stick tilt (if gamepad) gives analog speed: half-tilt = walk, full-tilt = run.
- [x] Mario faces his movement direction with smooth rotation.
- [x] Debug UI shows state transitions: IDLE → WALKING → RUNNING as speed increases.
- [x] Releasing all input: Mario decelerates and returns to IDLE.
- [x] Sharp reversal at high speed triggers SKIDDING state briefly, then IDLE.
- [x] Space (or A button) makes Mario jump. Height varies with how long the button is held.
- [x] Mario falls back down with gravity. Lands on Y=0 and returns to LANDING → IDLE.
- [x] Mario cannot jump again mid-air (only one jump per ground contact).
- [x] Camera orbits Mario. Right-click + drag (or right stick) rotates camera horizontally/vertically.
- [x] Camera auto-centers behind Mario's facing direction when not manually controlled.
- [x] Camera pitch clamps between ~10° and ~70°.
- [x] Fly camera (key 2) still works and returns to orbit camera when toggled off.

### Core Mechanics (Phases 3–7)

#### Phase 3 — Full Movement Set & Animation

All jump variants (double, triple, long, backflip, side somersault). Crouch, crawl. Punch combo, kick, dive, slide kick. Jump buffering + coyote time. Skeletal animation system driven by the state machine (~30 states). Mario animations from Blender.

**Jump Variants:**
- [x] Single jump: Space from idle or moving. Mario rises and falls. State shows SINGLE_JUMP → FREEFALL → LANDING.
- [x] Double jump: Jump, land, immediately jump again. State shows DOUBLE_JUMP. Noticeably higher than single.
- [x] Triple jump: Jump, land, jump, land, jump while running (speed > 12). State shows TRIPLE_JUMP. Highest jump. Fails if too slow or too long between landings.
- [x] Long jump: Hold Ctrl + Space while running. Low arc, long horizontal distance. State shows LONG_JUMP. No air steering.
- [x] Backflip: Hold Ctrl + Space while stationary/slow. Very high vertical jump. State shows BACKFLIP.
- [x] Side somersault: Run, reverse direction to trigger skid, then jump during skid. State shows SIDE_SOMERSAULT.
- [x] Jump chain resets: Doing a long jump, backflip, ground pound, dive, or any non-single/double jump between landings breaks the chain — next jump is always single.

**Input Buffering:**
- [x] Jump buffer: Press jump slightly before landing (~4 frames). Jump fires on contact instead of being eaten.

**Ground Pound:**
- [x] Jump, then press Ctrl mid-air. Mario pauses briefly (GROUND_POUND_SPIN), then slams down fast (GROUND_POUND_FALL).
- [x] On landing: brief stun (GROUND_POUND_LAND), then IDLE. Cannot move during stun.
- [x] Ground pound cannot be cancelled once started — pressing jump or other inputs during the fall does nothing.
- [x] Ground pound breaks jump chain.

**Crouch & Crawl:**
- [x] Hold Ctrl on the ground: Mario enters CROUCHING. Decelerates to a stop.
- [x] Ctrl + move: Mario enters CRAWLING. Slow movement (speed ~3). Facing follows input.
- [x] Release Ctrl while crouching/crawling: returns to IDLE.
- [x] Ctrl + Space while crouching still → backflip. Ctrl + Space while crawling fast → long jump.

**Combat Moves:**
- [x] Press B on ground: PUNCH_1. Press B again quickly: PUNCH_2. Again: PUNCH_3_KICK. Full 3-hit combo.
- [x] Wait too long (~10 frames) between presses: combo resets. Next B starts at PUNCH_1.
- [x] Jump + B while moving fast: DIVE. Mario lunges forward horizontally. On landing: BELLY_SLIDE (slides along ground with friction, then recovers).
- [x] Jump + B while slow/stationary: JUMP_KICK. Mario kicks in air. Normal landing.
- [x] Crawl + B while moving: SLIDE_KICK. Mario does a low sliding kick with slight hop.

**Skeletal Animation:**
- [x] Mario visibly animates (not a static mesh). Idle has subtle breathing motion.
- [x] Walk and run show leg/arm cycling. Run is faster cadence than walk.
- [x] Each jump variant has a distinct visual: single = arms up, double = flip, triple = big spin, long = horizontal stretch, backflip = backward flip.
- [x] Ground pound shows spin → ball curl → impact pose.
- [x] Crouch visibly lowers Mario. Crawl shows a crawling motion.
- [x] Punch combo shows left jab → right jab → spinning kick.
- [x] Transitions between animations blend smoothly (no teleporting between poses).
- [x] Debug UI shows animation clip index, time, and blend alpha updating in real time.

#### Phase 4 — World Collision & Physics

Collision mesh loading (separate simplified glTF per level). Uniform-grid spatial partition. Wall detection + sliding. Ceiling detection + head bonk. Slope behavior (walkable < 30°, steep 30–50°, wall > 50°). Surface types (normal, ice). Step-up tolerance for small ledges.

- [ ] Collision mesh loads from a separate simplified GLB (logged on startup).
- [ ] Mario walks on level geometry surfaces, not just Y=0.
- [ ] Coyote time: Walk off an edge, press jump within ~5 frames. Mario still gets a grounded jump instead of falling.
- [ ] Without coyote: Walk off edge, wait a moment, press jump — nothing happens (Mario freefalls).
- [ ] Mario slides along walls instead of stopping dead or passing through.
- [ ] Mario bonks on ceilings during jumps (velocity zeroed, starts falling).
- [ ] Walkable slopes (< 30°): Mario walks up and down normally.
- [ ] Steep slopes (30–50°): Mario slides downhill if standing still. Can run up if fast enough.
- [ ] Wall-like slopes (> 50°): Mario cannot stand on them, treated as walls.
- [ ] Small ledges (< 0.3 units): Mario steps up without jumping.
- [ ] Mario snaps to ground on downward slopes (doesn't hover or bounce).
- [ ] Ice surface: greatly reduced friction, Mario slides and turns sluggishly.
- [ ] Debug collision overlay (if implemented): wireframe of collision mesh visible.

#### Phase 5 — Advanced Movement & Interactions

Wall jump. Ledge grab + climb. Pole/tree climbing. Object carrying + throwing. Platform types: moving (Mario inherits velocity), falling (delay + respawn), tilting, one-way, breakable (ground pound).

- [ ] Wall jump: Jump toward a wall, press A near it. Mario kicks off away from the wall. Can chain wall jumps between parallel walls.
- [ ] Ledge grab: Jump near a platform edge. Mario catches the ledge and hangs. Press A or stick up to climb up. Press down or Z to drop.
- [ ] Pole/tree climbing: Jump onto a pole/tree. Mario grabs and can climb up/down with stick. Press A to jump off.
- [ ] Object carry: Press B near a carriable object. Mario picks it up. Movement is slower, no punching/diving. Press B to throw, Z to drop.
- [ ] Moving platform: Stand on it. Mario moves with the platform. Jump off and land back on = still inherits velocity.
- [ ] Falling platform: Stand on it. Brief delay, then it falls. Respawns after a timer.
- [ ] Tilting platform: Mario's position on the platform affects its tilt. Tips Mario off if angled too far.
- [ ] One-way platform: Mario can jump through from below, lands on top as solid.
- [ ] Breakable platform: Ground pound on it destroys it.

#### Phase 6 — Health, Damage & HUD

8-segment power meter. Damage from contact/hazards (1–3 segments). Knockback impulse + invincibility frames. Fall damage threshold. Void/pit instant death. Coins (yellow/red/blue) as collectibles + healing. Spinning Heart. HUD overlay via gl2d: health, coin counter, star counter, lives.

- [ ] HUD visible: power meter (top-left), star counter, coin counter (top-right).
- [ ] Power meter shows 8 segments at full health.
- [ ] Touching an enemy: Mario takes damage (1 segment), knockback impulse, invincibility frames (flashing) for ~2 seconds.
- [ ] During invincibility: no further damage from any source.
- [ ] Falling from height > ~11 units: fall damage (3 segments) + brief stun on landing.
- [ ] Falling into void (below the level): instant death regardless of health.
- [ ] Collecting yellow coin: +1 health segment, +1 coin counter.
- [ ] Collecting red coin: +2 health, +2 coins. Collecting blue coin: +5 health, +5 coins.
- [ ] Spinning Heart: running through restores health continuously.
- [ ] At 0 health: Mario death animation, lose a life. Lives counter decrements.
- [ ] Power meter flashes red at 2 or fewer segments.
- [ ] Course name/star name appears at bottom-center on course entry, fades after ~3 seconds.

#### Phase 7 — Swimming & Water

Surface swimming (A to paddle). Underwater free movement (A for burst, stick for pitch/yaw). Reduced underwater gravity. Air meter (health meter drains ~1 segment per 4s). Air replenished by coins or surfacing. Drowning death. Water plane rendering (transparent quad). Camera: looser underwater follow, expanded pitch range.

- [ ] Water plane renders as a transparent quad at the correct height.
- [ ] Mario transitions to surface swimming on entering water. Press A to paddle forward.
- [ ] Press Z on the surface: Mario dives underwater.
- [ ] Underwater: A gives burst of speed. Stick controls pitch and yaw. Free 3D movement.
- [ ] Health meter drains ~1 segment per 4 seconds while submerged (air meter).
- [ ] Surfacing or collecting coins underwater replenishes air.
- [ ] At 0 air/health: Mario drowns (death).
- [ ] Reduced gravity underwater (Mario sinks slowly, not fast).
- [ ] Camera follows more loosely underwater, expanded vertical pitch range.
- [ ] Exiting water: Mario transitions back to ground movement. Appropriate splash effect state.

### First Playable Loop (Phases 8–12)

#### Phase 8 — Entity System & Core Enemies

Base `Entity` class with optional reusable components (Physics, AI, Health, Animator). Spawn/despawn by distance. AI behaviors: patrol (back-and-forth or loop), detection radius, chase (direct movement toward Mario), terrain-edge respect. Implement: Goomba (patrol → charge → squished), Bob-omb (patrol → chase → explode; grabbable from behind), Koopa Troopa (patrol → flee; shell drop), Boo (approach when Mario's back is turned, intangible when faced). Generic defeat → coin drop.

- [ ] Enemies spawn when Mario approaches (within ~50 units) and despawn when Mario is far away.
- [ ] Goomba: patrols a path. Charges Mario when close. Jump on it to defeat. Drops a yellow coin.
- [ ] Bob-omb: patrols, chases Mario, explodes after a delay. Can be grabbed from behind and thrown.
- [ ] Koopa Troopa: patrols, flees from Mario. Any attack defeats it. Drops shell (separate object). Drops blue coin.
- [ ] Boo: approaches when Mario's back is turned. Stops/becomes transparent when Mario faces it. Defeat by turning and punching.
- [ ] Ground enemies respect terrain edges (stop and turn at ledges, don't walk off).
- [ ] Enemies that are defeated respawn when Mario leaves and re-enters their spawn radius.
- [ ] Enemy contact damages Mario (if not attacking or invincible).
- [ ] Attacking an enemy while jumping/punching/diving defeats it without taking damage.

#### Phase 9 — Audio System

Thin raudio wrapper: one music stream (OGG, crossfade on transitions), SFX array (WAV, fire-and-forget). Generate initial SFX via Python scripts (movement: footsteps, jumps, land; combat: punch, kick, hit; collectibles: coin, star appear). Generate first music tracks via Python/MIDI pipeline (overworld, boss fight). Hook SFX into existing movement and combat code. Footstep frequency scales with speed.

- [ ] Background music plays on startup (overworld track).
- [ ] SFX play for: jump, land, footsteps, punch, coin collect.
- [ ] Footstep SFX frequency scales with Mario's speed (faster = more frequent).
- [ ] Music crossfades when changing areas (~1 second transition).
- [ ] Multiple SFX can play simultaneously (e.g., coin + footstep).
- [ ] No audio crackling, popping, or distortion during normal gameplay.
- [ ] Muting/pausing: music stops on pause, resumes on unpause.

#### Phase 10 — Bob-omb Battlefield (First Playable Course)

Level geometry + collision mesh from Blender script. Object placement via JSON. All 7 stars: King Bob-omb boss (grab from behind, throw 3×, must stay on summit), red coins, 100-coin star, Chain Chomp (ground-pound post to free), cannon (Bob-omb Buddy NPC unlocks, aim-and-launch UI), remaining mission stars. Star select screen on course entry. Course exit on star collection.

- [ ] Course geometry loads and renders. Mario can walk around the entire level.
- [ ] Collision mesh matches visual geometry (no falling through ground, walking through walls).
- [ ] Star select screen appears when entering the course. Shows 6 mission names.
- [ ] King Bob-omb boss: on the summit. Grab from behind, throw 3 times to defeat. Must throw on summit. Awards a star.
- [ ] Red coins: 8 scattered in the level. Collecting all 8 spawns a star.
- [ ] 100-coin star: Reaching 100 coins spawns a star at Mario's location.
- [ ] Chain Chomp: ground-pound the post to free it. Awards a star.
- [ ] Cannon: Talk to Bob-omb Buddy NPC to unlock. Enter cannon, aim and launch Mario.
- [ ] Collecting a star: plays star collect sequence, exits course, returns to hub/menu.
- [ ] Course exit saves progress (collected star is tracked).

#### Phase 11 — Hub World: Peach's Castle

Castle exterior (grounds, moat, courtyard) + interior (first floor, basement structure, second floor locked). Star doors (gated by star count: 1, 3, 8, 12, 30, 50, 70). Painting portals → star select → course load. Toad NPCs (dialog on interact). Regular doors. Basement accessible after Phase 12. Second floor accessible after Phase 14.

- [ ] Castle exterior renders: grounds, moat, courtyard.
- [ ] Castle interior: first floor accessible. Doors work (walk into to enter).
- [ ] Star doors display required star count. Cannot open without enough stars.
- [ ] Painting portals: jumping into a painting opens the star select screen, then loads the course.
- [ ] Toad NPCs: stand near and press B to talk. Dialog text appears.
- [ ] Star doors unlock at correct thresholds: 1, 3, 8, 12, 30, 50, 70.
- [ ] Basement door is locked until the key from Phase 12 is obtained.

#### Phase 12 — Bowser in the Dark World

Linear obstacle-course level (platforms, flames, moving hazards). Bowser boss arena: circular platform with 5 spiked bombs. Grab-spin-throw mechanic (stick rotation builds angular momentum, release direction matters). Bowser AI: fire breath (arc toward Mario), charge (sidestep window). 1 hit to defeat. Reward: basement key.

- [ ] Level loads: linear obstacle course with platforms, flames, moving hazards.
- [ ] Mario can traverse the level to reach the boss arena.
- [ ] Bowser arena: circular platform with 5 spiked bombs around the edge.
- [ ] Bowser AI: fire breath (arc toward Mario), charge (pause after, window to get behind).
- [ ] Grab Bowser's tail with B from behind. Rotate stick to spin. Release B to throw.
- [ ] Throwing Bowser into a spiked bomb: damage, bomb explodes. 1 hit to defeat.
- [ ] Missing the bomb: Bowser lands on arena or falls off and jumps back. Resumes attacking.
- [ ] Defeating Bowser: basement key awarded. Cutscene/dialog plays.
- [ ] Returning to castle: basement door is now unlockable.

### Course Expansion (Phases 13–18)

#### Phase 13 — Courses 2–5

Whomp's Fortress (Whomps, Whomp King boss, Thwomps, Piranha Plants, poles). Jolly Roger Bay (underwater exploration, Unagi eel, sunken ship). Cool, Cool Mountain (ice surface physics, slide course, penguin race/escort). Big Boo's Haunt (Boo mechanics, Big Boo boss, Mr. I). Each course: Blender geometry, collision mesh, 7 stars, course-specific enemies.

- [ ] Each course loads with correct geometry and collision.
- [ ] Whomp's Fortress: Whomps fall forward (dodge and ground-pound back). Whomp King boss (3 hits). Thwomps slam. Piranha Plants sleep/bite. Poles climbable.
- [ ] Jolly Roger Bay: underwater areas functional. Unagi eel present. Sunken ship accessible.
- [ ] Cool, Cool Mountain: ice physics on snow surfaces. Slide course works (enter at top, ride to bottom). Penguin race/escort mission functional.
- [ ] Big Boo's Haunt: Boo mechanics work (transparency when faced). Big Boo boss (3 hits). Mr. I (run circles to defeat).
- [ ] All 7 stars per course are obtainable and tracked.

#### Phase 14 — Bowser in the Fire Sea

Second Bowser level (lava hazards, tilting platforms). Bowser adds ground-pound shockwave (expanding ring, jump to dodge). Faster movement. Arena tilts after taking damage. 1 hit. Reward: second floor key. Unlocks castle second floor and upper courses.

- [ ] Lava hazards deal damage (3 segments) + upward knockback on contact.
- [ ] Tilting platforms work.
- [ ] Bowser adds ground-pound shockwave (expanding ring). Jumpable.
- [ ] Bowser moves faster than fight 1.
- [ ] Arena tilts after Bowser takes damage.
- [ ] 1 hit to defeat. Awards second floor key.
- [ ] Second floor of castle now accessible.

#### Phase 15 — Courses 6–9

Hazy Maze Cave (underground, Dorrie, toxic maze). Lethal Lava Land (lava surface type, Bully/Big Bully boss, rolling log). Shifting Sand Land (desert, pyramid interior, Eyerok boss, Pokey, quicksand surfaces). Dire, Dire Docks (underwater dock, submarine, poles). Course-specific enemies: Bully, Eyerok, Pokey, Monty Mole, Dorrie. Heave-Ho.

- [ ] Hazy Maze Cave: underground areas, Dorrie rideable, toxic maze functional.
- [ ] Lethal Lava Land: lava surface type works. Bully/Big Bully pushable into lava. Rolling log traversable.
- [ ] Shifting Sand Land: quicksand surfaces (shallow = slow sink, deep = death). Pyramid interior. Eyerok boss.
- [ ] Dire, Dire Docks: underwater dock area. Poles climbable.
- [ ] All course-specific enemies functional (Bully, Eyerok, Pokey, Monty Mole, Dorrie, Heave-Ho).

#### Phase 16 — Cap Power-Ups & Secret Stages

Wing Cap (flight on triple jump/cannon: stick up = dive, stick down = climb; 60s duration). Metal Cap (invulnerable, heavy, walk underwater with infinite air; 20s). Vanish Cap (semi-transparent, pass through specific walls/grates/enemies; 20s). Three cap switch stages (accessed from castle hub). Cap blocks in courses (only dispense after switch pressed). Combined caps where applicable.

- [ ] Three cap switch stages accessible from castle hub.
- [ ] Pressing a cap switch: corresponding colored cap blocks now dispense caps in all courses.
- [ ] Wing Cap: 60-second timer. Triple jump or cannon → flight mode. Stick up = dive, stick down = climb. Touching ground cancels flight.
- [ ] Metal Cap: 20-second timer. Invulnerable. Walk underwater with infinite air.
- [ ] Vanish Cap: 20-second timer. Semi-transparent. Walk through specific walls and grates.
- [ ] Cap timer: warning beeps in last 5 seconds. Cap disappears when timer expires.
- [ ] Combined caps work where applicable (both effects active).
- [ ] Cap blocks only dispense after the switch is pressed. Before pressing: block is solid but gives nothing.

#### Phase 17 — Courses 10–15

Snowman's Land (giant snowman, wind zone surface type, ice). Wet-Dry World (water level manipulation mechanic). Tall, Tall Mountain (vertical course, Fly Guy, Ukiki). Tiny-Huge Island (scale-switching between tiny and huge versions). Tick Tock Clock (moving gears/platforms synced to clock time on entry). Rainbow Ride (magic carpet ride, floating platforms). Enemies: Fly Guy, Amp, Lakitu, Spiny, Wiggler boss.

- [ ] Snowman's Land: wind zone pushes Mario. Ice surfaces. Giant snowman obstacle.
- [ ] Wet-Dry World: water level changes based on crystal taps or entry height.
- [ ] Tall, Tall Mountain: vertical course traversable. Fly Guy carries items. Ukiki steals cap.
- [ ] Tiny-Huge Island: entering from different pipes switches between tiny and huge scale. Both versions functional.
- [ ] Tick Tock Clock: platform/gear speed depends on minute hand position at entry. 12 o'clock = frozen.
- [ ] Rainbow Ride: magic carpet follows a rail path. Floating platforms.
- [ ] All enemies functional (Fly Guy, Amp, Lakitu, Spiny, Wiggler boss).

#### Phase 18 — Bowser in the Sky

Final Bowser level (longest obstacle course, all hazard types). Bowser uses all attacks: fire breath, charge, ground-pound shockwave, fire rain (fireballs from sky with shadow indicators). 3 hits to defeat. After hit 2, arena outer edge crumbles to star shape (fewer bombs, smaller arena). Third throw needs more spin momentum. Reward: Grand Star → ending sequence.

- [ ] Longest obstacle course. All hazard types from prior phases.
- [ ] Bowser uses all attacks: fire breath, charge, ground-pound shockwave, fire rain.
- [ ] Fire rain: fireballs fall from sky. Shadow indicators on ground show landing zones.
- [ ] 3 hits to defeat. After hit 2: arena edge crumbles to star shape. Fewer bombs, smaller arena.
- [ ] Third throw requires more spin momentum (more stick rotations).
- [ ] Final defeat: Grand Star awarded. Ending sequence triggers.

### Game Flow & Polish (Phases 19–20)

#### Phase 19 — Menus, Save System & Game Flow

Title screen (castle background, "Press Start", idle demo optional). File select (4 slots, star count display, new/continue/copy/delete). Pause menu (continue/exit course). Game Over screen (continue → file select, quit → title). Save system: raw struct serialization (`save_N.dat`), magic number + version field, saves on star collection + course exit. Lives system: start at 4, 1-Up mushrooms, extra lives from coins (50 + 100), Game Over at 0.

- [ ] Title screen: castle background, logo, "Press Start" prompt. Any button → file select.
- [ ] File select: 4 slots. Empty slots show "New Game". Populated slots show star count.
- [ ] New game: intro sequence, Mario placed outside castle.
- [ ] Pause menu: press Start during gameplay. Game freezes. Continue/Exit Course options.
- [ ] Exit Course: returns to castle, keeps collected stars, loses current coins.
- [ ] Game Over screen at 0 lives: Continue (→ file select, lives reset to 4) or Quit (→ title).
- [ ] Save triggers on star collection + course exit. Relaunch game: progress persists.
- [ ] Copy/delete save file works from file select.
- [ ] Lives: start at 4. 1-Up mushroom = +1 life. 50 coins = +1 life. 100 coins = +1 life + star.

#### Phase 20 — Polish & Completion

Secret courses: Princess's Secret Slide, The Secret Aquarium, Wing Mario Over the Rainbow, Cavern of the Metal Cap, Vanish Cap Under the Moat. Castle secret stars: MIPS the rabbit (2 stars at different star counts), Toad gifts, hidden slide stars. Ending sequence: Bowser defeated cutscene, Peach rescued, cake scene. 120-star bonus: Yoshi on castle rooftop (99 lives + message). Remaining SFX/music/jingles. Final tuning pass on movement parameters.

- [ ] Secret courses accessible: Princess's Secret Slide, Secret Aquarium, Wing Mario Over the Rainbow, Metal/Vanish cap caverns.
- [ ] MIPS the rabbit: appears in basement at certain star counts. Catchable. Awards 2 stars.
- [ ] Toad gifts: specific Toads give stars when talked to.
- [ ] Ending sequence plays after Bowser 3: Peach rescued, cake scene.
- [ ] 120-star bonus: Yoshi on castle rooftop. Gives 99 lives + unique message.
- [ ] All SFX present and hooked up (no silent actions).
- [ ] All music tracks present (no silent areas).
- [ ] Full playthrough: title → file select → all 120 stars → ending. No crashes or softlocks.

