# Super Mario 64 — Game Specification

A simplified recreation of Super Mario 64 focused on core gameplay mechanics with simple 3D graphics. Not a pixel-perfect clone — the goal is to capture the feel and fun of the original through faithful movement, physics, and level design principles.

## Rendering Approach

- **Resolution:** 1200x900 (4:3 aspect ratio). Windowed by default.
- **3D world:** Direct OpenGL 3.3 Core for the game world. Levels built from basic geometry (cubes, ramps, cylinders). Characters and enemies are simple meshes or billboarded sprites. No complex texturing or lighting required — flat/vertex colors and basic diffuse shading are sufficient.
- **2D overlays:** gl2d + glui for all HUD elements (health meter, coin counter, star count, lives) and game menus (title screen, file select, star select, pause menu).

## Rendering Pipeline

### 3D Pipeline (OpenGL 3.3 Core)

Each frame renders in this order:

1. **Clear** — Clear color buffer (sky color or skybox) and depth buffer.
2. **Skybox** — Render a simple skybox or gradient background (fullscreen quad, depth disabled). Per-course sky color/texture.
3. **Opaque geometry** — Render all level geometry and solid objects with depth testing enabled. Uses a basic shader: vertex position, normal, color (or simple texture). Lighting: single directional light + ambient. No shadows required initially.
4. **Characters and enemies** — Render Mario, enemies, NPCs. Either simple 3D meshes or billboarded sprites that always face the camera. Animated via sprite sheets (billboards) or simple skeletal/vertex animation (meshes).
5. **Transparent geometry** — Render water surfaces, glass, Vanish Cap Mario with alpha blending. Draw back-to-front or use a simple depth-peel approach. Coins and particle effects also go here.
6. **Particles** — Coin sparkles, dust on landing, splash effects, fire, star effects. Billboarded quads with alpha.
7. **2D overlay** — Switch to orthographic projection. gl2d + glui render the HUD and any active menu on top of the 3D scene. Depth testing disabled.

### View and Projection

- **Projection:** Perspective matrix — FOV ~45 degrees, near plane 0.1, far plane 500.
- **View:** Computed from camera position and target point (see Camera System). glm::lookAt.
- **Model:** Each object has a model matrix (position, rotation, scale). Sent as a uniform to the shader.

### Mesh Loading

- Level geometry and character meshes loaded from OBJ files (or a simpler custom format). Parsed at load time into vertex buffer objects (VBOs) with position, normal, and UV/color attributes.
- Levels are static meshes — loaded once per course, drawn each frame.
- Animated characters: either swap between pre-made meshes per frame (sprite-sheet style) or use simple bone-based animation with a vertex shader.

### Frustum Culling

- Objects outside the camera frustum are skipped. Simple sphere-based check per object (bounding sphere vs. 6 frustum planes).
- Level geometry can be split into chunks/sectors for coarser culling on larger courses.

## Input

Supports keyboard + mouse and gamepad. GLFW handles both natively.

- **Gamepad:** Left stick for movement (analog), right stick for camera, face buttons for actions. Analog stick deflection controls walk/run speed.
- **Keyboard:** WASD for movement (digital — walk/run toggle or always run), mouse for camera. Movement is 8-directional; no analog speed control.
- Input is abstracted into game actions (move, jump, attack, crouch, camera) so all gameplay code is input-method-agnostic. Bindings are remappable.

## Core Movement

Mario's movement is analog — walking speed is proportional to stick deflection. Full tilt = run. Mario accelerates and decelerates smoothly, not instantly. Turning at high speed causes a skid/brake animation before reversing direction.

### Ground Moves

| Move | Input | Description |
|------|-------|-------------|
| Walk / Run | Stick tilt | Speed proportional to stick deflection |
| Crouch | Z (hold) | Lowers hitbox, enables crouch moves |
| Crawl | Z + stick | Slow movement while crouched |
| Side-step | Stick toward wall edge | Shimmy along narrow ledges |

### Jumps

| Move | Input | Description |
|------|-------|-------------|
| Single Jump | A | Height varies with hold duration; max height scales with speed |
| Double Jump | A on landing from jump | Higher than single jump |
| Triple Jump | A on landing from double jump | Highest jump; requires running speed (minimum threshold) |
| Long Jump | Z + A while running | Low arc, long horizontal distance |
| Backflip | Z + A while stationary | High vertical jump from crouch |
| Side Somersault | Reverse stick + A | Flick stick opposite to run direction, then jump |
| Wall Jump | A near wall while airborne | Kick off vertical surfaces; Mario must face ~45 degrees of wall |
| Ground Pound | Z while airborne | Flip and slam straight down; high damage, breaks some objects |

### Combat Moves

| Move | Input | Description |
|------|-------|-------------|
| Punch (x3 combo) | B, B, B on ground | Three-hit combo: punch, punch, kick |
| Jump Kick | B while airborne (low speed) | Mid-air kick |
| Dive | B while airborne (high speed) | Forward dive; slides on ground after landing |
| Slide Kick | B while crouching + moving | Low sliding kick along the ground |

### Other Actions

| Move | Input | Description |
|------|-------|-------------|
| Grab / Pick up | B near object | Grab objects, enemies, ledges |
| Throw | B while holding object | Throw carried object forward |
| Ledge grab | Auto | Mario grabs ledges when jumping near them |
| Climb | Auto near pole/tree | Climb vertical poles; jump off with A |
| Swim (surface) | A to stroke | Paddle on water surface |
| Swim (underwater) | A to stroke, stick to steer | Free 3D movement; health meter doubles as air meter |
| Carry object | B near carriable object | Limits moveset: no punching/diving; slower movement. Press B to throw, Z to drop. |

## Collision System

### Collision Layers

Bitmask-based layer system. Each collider has a category (what it is) and a mask (what it collides with). Collision only occurs when `A.mask & B.category` is nonzero. Layer assignments to be defined during implementation.

### Collision Shapes

- **Mario:** Vertical capsule (cylinder + hemisphere caps). Approximate dimensions: radius ~0.5 units, total height ~1.8 units standing, ~0.9 units crouching.
- **Enemies:** Axis-aligned bounding boxes (AABB) or spheres depending on shape. Simple enemies (Goomba, Bob-omb) use spheres; tall/wide enemies (Whomp, Thwomp) use AABB.
- **Level geometry:** Triangle mesh collision. Each triangle stores its surface type (normal, ice, lava, sand, etc.) and a surface normal for slope calculations.
- **Triggers/collectibles:** Sphere triggers (coins, stars, hazard zones, warp points).

### Ground Detection

- Downward raycast from Mario's center, length slightly longer than step-up height.
- Returns: ground hit (yes/no), ground normal, surface type, ground Y position.
- **Step-up tolerance:** Mario can walk up small ledges (< ~0.3 units) without jumping. Raycast accounts for this.
- **Snap-to-ground:** While grounded and not jumping, Mario snaps to the ground Y each frame to stay attached on downward slopes and moving platforms.
- **Coyote time:** ~5 frames after walking off an edge, Mario can still jump as if grounded.

### Wall Detection

- Horizontal sphere/capsule sweep in Mario's movement direction each frame.
- On wall contact: Mario slides along the wall surface (project velocity onto the wall plane).
- Wall normals are stored for wall-jump eligibility checks (~45 degrees from vertical = wall-jumpable).
- Prevents Mario from walking through geometry or into corners.

### Ceiling Detection

- Upward raycast from Mario's head position.
- If ceiling is hit during a jump: Mario's vertical velocity is zeroed (head bonk), begins falling immediately.
- Must also check when standing up from crouch in tight spaces.

### Platform Types

| Type | Behavior |
|------|----------|
| Static | Fixed geometry, standard collision |
| Moving | Translates on a path; Mario inherits platform velocity while grounded on it |
| Falling | Begins falling a short delay after Mario stands on it; respawns after a timer |
| Tilting | Rotates based on Mario's position on it; tips Mario off if angled too far |
| One-way | Passable from below, solid from above (used for some elevated platforms) |
| Breakable | Destroyed by ground-pound; may reveal items or paths below |

## Physics

### Gravity and Momentum

- Gravity is a constant downward acceleration applied every frame while airborne: ~-38 units/s².
- Terminal velocity caps falling speed: ~-75 units/s.
- Mario preserves horizontal momentum into jumps. Faster run = longer jump arc.
- Air control: Mario can steer horizontally while airborne but with reduced authority (~30% of ground acceleration).
- Ground pound overrides horizontal velocity to zero and applies a faster fall speed (~2x gravity).

### Variable Jump Height

- Pressing jump applies an initial upward velocity (varies by jump type, see table below).
- Holding the jump button reduces effective gravity during the rising phase (~60% normal gravity), allowing higher jumps.
- Releasing the jump button early restores full gravity immediately, cutting the jump short.
- Double and triple jumps have fixed initial velocities that are progressively higher.

### Movement Parameters

All values are tuning targets — adjust during playtesting. Expressed relative to Mario's height (~1.8 units).

| Parameter | Value | Notes |
|-----------|-------|-------|
| Walk speed (max) | ~8 units/s | At ~50% stick deflection |
| Run speed (max) | ~16 units/s | At full stick deflection |
| Ground acceleration | ~24 units/s² | Time to full run: ~0.7s |
| Ground deceleration | ~32 units/s² | Faster than acceleration (snappy stop) |
| Ground friction (normal) | High | Mario stops quickly when stick released |
| Ground friction (ice) | ~20% of normal | Long slide, delayed turning |
| Air acceleration | ~7 units/s² | ~30% of ground |
| Single jump velocity | ~22 units/s | ~2.5x Mario height at peak (button held) |
| Double jump velocity | ~28 units/s | ~3.5x Mario height |
| Triple jump velocity | ~33 units/s | ~4.5x Mario height; requires speed > 12 units/s |
| Long jump velocity | ~15 vertical, ~32 horizontal | Low arc, long distance |
| Backflip velocity | ~35 units/s | Highest vertical, minimal horizontal |
| Wall jump velocity | ~22 vertical, ~16 horizontal | Away from wall |
| Gravity | ~38 units/s² | |
| Terminal velocity | ~75 units/s | |
| Swim speed | ~10 units/s | Each A press gives a speed burst |

### Slopes

- Surface normal angle determines slope behavior:
  - **< 30 degrees:** Normal movement. Mario can walk, run, and stand freely.
  - **30–50 degrees:** Steep slope. Mario slides downhill if stationary or moving slowly. Can still run up if at sufficient speed.
  - **> 50 degrees:** Wall-like. Treated as a wall for collision purposes; Mario cannot stand on it.
- Sliding down a steep slope accelerates Mario in the downhill direction. Jumping while sliding is allowed.

### Surface Types

| Surface | Effect |
|---------|--------|
| Normal | Standard friction and behavior |
| Ice | Greatly reduced friction; Mario slides, turning is sluggish |
| Lava | Instant damage (3 segments) + upward knockback launch on contact |
| Quicksand (shallow) | Mario sinks slowly; reduced movement speed; jump to escape |
| Quicksand (deep) | Mario sinks; instant death if fully submerged |
| Wind zone | Constant horizontal force applied while Mario is in the area |

### Knockback and Damage

- Taking damage knocks Mario backward with a fixed impulse (~12 units/s).
- Mario gets ~2 seconds of invincibility frames after taking damage (flashing visual). Cannot take further damage during this window.
- Fall damage threshold: ~11 units of vertical distance (~6x Mario's height). Deals 3 segments and a brief stun on landing.
- Falling into a void / bottomless pit = instant death (lose a life regardless of health).

### Water Physics

- Mario floats on the surface by default. A button paddles forward on the surface.
- Diving underwater (Z on surface): free 3D movement. A button gives a speed burst in the facing direction. Stick controls pitch and yaw.
- Gravity is greatly reduced underwater (~15% of normal).
- The health meter doubles as an air meter while submerged. Wedges drain at ~1 segment per 4 seconds.
- Air replenished by collecting coins underwater or surfacing.
- At zero air/health: Mario drowns (lose a life).
- Metal Cap: Mario sinks to the bottom and walks normally with infinite air, normal gravity.

## Movement State Machine

Mario's action state determines which moves are available and how physics are applied. Only one state is active at a time.

```
IDLE ──────── stick input ───────► WALKING/RUNNING
  │                                    │
  ├── A ──► SINGLE_JUMP                ├── A ──────────► SINGLE_JUMP
  ├── Z ──► CROUCHING                  ├── Z + A ──────► LONG_JUMP
  └── B ──► PUNCH_COMBO                ├── reverse + A ► SIDE_SOMERSAULT
                                       └── Z ──────────► SLIDE_KICK (if B) / CROUCHING
CROUCHING
  ├── A ──► BACKFLIP
  ├── stick ► CRAWLING
  └── release Z ► IDLE

SINGLE_JUMP
  ├── land ► IDLE (or WALKING if stick held)
  ├── land + A ► DOUBLE_JUMP
  ├── Z ──► GROUND_POUND
  ├── B (low speed) ► JUMP_KICK
  ├── B (high speed) ► DIVE
  ├── near wall + A ► WALL_JUMP
  └── near ledge ► LEDGE_GRAB

DOUBLE_JUMP
  ├── land + A (if fast enough) ► TRIPLE_JUMP
  └── (same airborne options as SINGLE_JUMP)

TRIPLE_JUMP ── (same airborne options, no further chain)

GROUND_POUND
  └── land ► IDLE (brief stun frames)

DIVE
  └── land ► BELLY_SLIDE ── stick/A ► recover to IDLE

LEDGE_GRAB
  ├── A or stick up ► LEDGE_CLIMB ► IDLE (on top)
  └── stick down or Z ► drop ► FREEFALL

SWIMMING_SURFACE
  ├── A ► paddle stroke
  └── Z ► SWIMMING_UNDERWATER

SWIMMING_UNDERWATER
  ├── A ► swim burst
  └── surface ► SWIMMING_SURFACE

KNOCKBACK ── (timer) ──► IDLE (invincibility frames active)
```

Key rules:
- **Jump buffering:** If A is pressed within ~4 frames before landing, the jump still registers on landing.
- **Coyote time:** ~5 frames after walking off an edge, a jump input still triggers a grounded jump instead of no action.
- **Combo timer:** The punch-punch-kick combo chain resets if B is not pressed within ~10 frames of the previous hit.
- **State priority:** Damage/knockback overrides any other state. Ground pound cannot be canceled once started.

## Health and Lives

### Power Meter

- 8-segment pie/wedge health display.
- Full health = 8 segments.
- Damage sources remove 1-3 segments depending on severity:
  - Enemy contact / small hazard: 1 segment
  - Stronger attacks / moderate hazard: 2 segments
  - Lava / long fall / crushing: 3 segments
- At 0 segments: Mario loses a life.

### Healing

- Yellow coin: restores 1 segment.
- Blue coin: restores 5 segments (worth 5 yellow coins).
- Red coin: restores 2 segments (worth 2 yellow coins toward 100-coin star).
- Spinning Heart: restores health continuously while Mario runs through it.
- Entering/touching water surface: restores health gradually.

### Coin Counter

- Each course visit has a coin counter that starts at 0 and resets on exit.
- Yellow coin = 1, Red coin = 2, Blue coin = 5.
- Reaching 100 coins spawns a Power Star at Mario's location (one per course).

### Lives

- Mario starts with 4 lives.
- 1-Up mushrooms grant an extra life.
- Collecting 100 coins in a course grants an extra life (in addition to the star).
- Collecting 50 coins in a course grants an extra life.
- Game Over at 0 lives — restart from file select screen.

## Camera System

Inspired by the original Lakitu camera, simplified for modern controls.

### Default Behavior

- Third-person camera positioned behind and above Mario.
- Follow distance: ~8 units behind, ~4 units above Mario's position.
- Camera target: a point slightly above Mario's center (chest height), not his feet.
- Interpolation: camera position and rotation lerp smoothly toward the target (~5-10% per frame). Never teleports except on scene transitions.
- Auto-centering: when the player is not manually rotating the camera, it slowly slerps to face the direction Mario is moving (~2% per frame). Does not fight the player — any manual input pauses auto-centering for ~1 second.

### Player Control

- **Horizontal orbit:** Right stick X (gamepad) or mouse X (keyboard) rotates the camera around Mario on the horizontal plane. Full 360-degree rotation.
- **Vertical angle:** Right stick Y (gamepad) or mouse Y (keyboard) adjusts pitch. Clamped to ~10 degrees (near ground level) to ~70 degrees (looking down from above).
- **Zoom:** Toggle or smooth zoom between close (~5 units) and far (~12 units) distance. Shoulder button or scroll wheel.
- **First-person look:** When stationary, a button press enters first-person mode. Camera moves to Mario's head position. Stick/mouse looks around freely. Mario cannot move in this mode. Releasing the button returns to third-person.

### Camera Collision

- Raycast from Mario to the desired camera position each frame.
- If the ray hits level geometry, move the camera forward to just in front of the hit point (+ small offset to avoid clipping into walls).
- When the obstruction clears, smoothly lerp back to the full follow distance (don't snap).
- In very tight spaces, the camera may push in close to Mario and raise its angle to look more top-down.

### Mode Transitions

- **Indoor/corridor:** When Mario enters a tight space (detected by ceiling height or trigger volume), camera pulls in closer and raises angle. Smooth transition over ~0.5 seconds.
- **Boss arena:** Camera may lock to a fixed position or fixed radius around the arena center. Still allows player rotation.
- **Underwater:** Camera follows more loosely with wider orbit. Vertical angle range is expanded (Mario can swim up/down freely).
- **Cannon aim:** Camera switches to first-person inside the cannon barrel. Stick aims the crosshair. Launching returns to third-person tracking the flight arc.
- **Cutscene/star collect:** Camera moves to a scripted position (e.g., looking at the star, looking at a door opening). Player input is locked during these moments.

## Level Structure

### Hub World — Peach's Castle

- Central hub connecting all courses.
- Three floors plus basement and courtyard.
- Courses accessed by jumping into paintings (or equivalent portals). Entering a painting opens a star select screen showing the 6 mission names; selected star can affect object/enemy placement in the level.
- Doors require star counts to open (1-star door, 3-star door, 8-star door, etc.).
- Key locations unlock as player progresses (basement key from Bowser 1, second floor key from Bowser 2).
- Exiting a course: collecting a star warps Mario out, or use the pause menu "Exit Course" (lose no progress except unsaved coins).

### Main Courses (15)

Each course has 7 Power Stars:
- 6 mission-based stars (each with a name/objective).
- 1 hidden star for collecting 100 coins.

| # | Course | Theme |
|---|--------|-------|
| 1 | Bob-omb Battlefield | Grassy hills, King Bob-omb boss |
| 2 | Whomp's Fortress | Vertical stone fortress, Whomp King boss |
| 3 | Jolly Roger Bay | Underwater sunken ship |
| 4 | Cool, Cool Mountain | Snowy slopes, slide, baby penguin |
| 5 | Big Boo's Haunt | Ghost house, Boos |
| 6 | Hazy Maze Cave | Underground caverns, Dorrie |
| 7 | Lethal Lava Land | Lava platforms, Bully boss |
| 8 | Shifting Sand Land | Desert, pyramid, Eyerok boss |
| 9 | Dire, Dire Docks | Underwater docks, submarine |
| 10 | Snowman's Land | Snow and ice, giant snowman |
| 11 | Wet-Dry World | Water level manipulation |
| 12 | Tall, Tall Mountain | Vertical mountain course |
| 13 | Tiny-Huge Island | Scale-switching (tiny/huge versions) |
| 14 | Tick Tock Clock | Clock interior, moving gears |
| 15 | Rainbow Ride | Floating platforms, magic carpet |

### Bowser Levels (3)

Linear obstacle-course levels ending in a Bowser boss fight:

1. **Bowser in the Dark World** — unlocked at ~8 stars. Rewards basement key.
2. **Bowser in the Fire Sea** — unlocked at ~30 stars. Rewards second floor key.
3. **Bowser in the Sky** — unlocked at 70 stars (endless stairs). Final boss, completes the game.

### Secret / Bonus Courses

- Cap switch stages (3): unlock Wing Cap, Metal Cap, Vanish Cap blocks.
- Princess's Secret Slide.
- Wing Mario Over the Rainbow.
- The Secret Aquarium.
- Cavern of the Metal Cap, Vanish Cap Under the Moat.
- Various castle secret stars (catching MIPS the rabbit, Toads, etc.).

### Star Progression

| Stars Required | Unlocks |
|----------------|---------|
| 1 | Whomp's Fortress door |
| 3 | Jolly Roger Bay / Cool, Cool Mountain door |
| 8 | Big star door to Bowser in the Dark World; defeating him yields the basement key |
| 12 | Basement star door (access to later basement courses) |
| 30 | Dire, Dire Docks door; Bowser in the Fire Sea accessible from there; defeating him yields the second floor key |
| 50 | Third floor star door |
| 70 | Bowser in the Sky (endless stairs bypass) |

**Total: 120 Power Stars** (105 from main courses + 15 from secret courses/castle).

## Cap Power-Ups

Activated by hitting colored cap blocks (must first press corresponding switch in a secret stage).

| Cap | Color | Duration | Effect |
|-----|-------|----------|--------|
| Wing Cap | Red | 60 seconds | Triple jump or cannon launch enters flight mode. Stick up = dive (gain speed, lose altitude), stick down = climb (lose speed, gain altitude). Touching ground cancels flight. |
| Metal Cap | Green | 20 seconds | Invulnerable, heavy. Walk underwater, immune to gas/wind. Cannot be grabbed by enemies. |
| Vanish Cap | Blue | 20 seconds | Semi-transparent. Walk through certain walls, metal grates, and enemies. Still affected by gravity. |

Caps can be combined (e.g., Metal + Vanish in certain levels) for both effects simultaneously.

## Enemies

### Enemy AI Framework

All enemies share a common behavior structure:

- **Spawn/despawn:** Enemies only exist when Mario is within a spawn radius (~50 units). They despawn when Mario moves far enough away and respawn at their original position when Mario returns (unless permanently defeated for a star objective).
- **Detection:** Enemies have a detection radius (~15 units default). When Mario enters this radius, the enemy switches from idle/patrol to its active behavior (chase, attack, etc.). Some enemies also require line-of-sight (raycast to Mario, blocked by walls).
- **Patrol:** Most ground enemies walk a simple path — either back-and-forth between two points or a looping route. They respect terrain edges (stop and turn at ledges) unless they are a flying or floating enemy.
- **Chase:** When activated, enemies move toward Mario's position at their chase speed. They do not pathfind — they walk directly toward Mario, stopping at walls and edges. If Mario leaves the detection radius, they return to patrol after a short delay.
- **Attack:** Contact damage is applied when Mario's capsule overlaps the enemy's collision shape during non-invincible, non-attacking states. Some enemies have a specific attack (Bob-omb explosion, Piranha Plant bite) with a windup and hitbox timing.
- **Defeat:** When defeated, enemies play a defeat animation, spawn their drop (coin, blue coin, star), and are removed. Most enemies respawn when Mario leaves and re-enters the spawn radius.
- **Terrain:** Ground enemies use the same ground detection raycasts as Mario to stay on surfaces and respect slopes. Flying enemies ignore ground collision and follow fixed or tracking paths in 3D.

### Common Enemies

| Enemy | Behavior | Defeat Method |
|-------|----------|---------------|
| Goomba | Walks a patrol route, charges Mario on sight | Jump on, punch, any attack. Drops yellow coin. |
| Bob-omb | Walks, chases Mario, explodes after fuse | Grab from behind and throw; or wait for self-destruct. |
| Koopa Troopa | Walks patrol, flees when Mario approaches | Any attack; leaves shell behind (rideable). Drops blue coin. |
| Boo | Becomes transparent/intangible when Mario faces it; approaches when Mario's back is turned | Turn around and punch when close, or ground-pound. |
| Bully | Charges at Mario to push him off platforms | Push back by attacking; knock into lava/off edge. No direct damage to Mario. |
| Chain Chomp | Lunges at Mario while chained to post | Cannot be defeated normally; ground-pound post to free it (star reward). |
| Piranha Plant | Sleeps when Mario is very close/still; wakes and bites when Mario approaches from distance | Attack while awake; tiptoe past while asleep. |
| Amp | Electric orb, orbits a fixed path | Cannot be defeated; avoid contact (electric damage). |
| Thwomp | Slams down when Mario walks beneath | Cannot be defeated; time passage between slams. |
| Whomp | Falls forward to crush Mario | Dodge the fall, ground-pound its back. |
| Lakitu (enemy) | Flies overhead, drops Spinies at Mario | Jump on or punch; respawns. |
| Mr. I | Giant eyeball, shoots projectiles | Run circles around it until it spins out and dies. |
| Pokey | Cactus segments, walks toward Mario | Punch segments off; destroy head to defeat. |
| Monty Mole | Pops out of ground, throws rocks | Jump on or punch. |
| Fly Guy | Flying Shy Guy, may carry items | Jump on or punch; may drop coin or 1-Up. |
| Unagi | Giant eel lurking in underwater tunnels | Cannot be defeated; grab the star on its tail. |
| Heave-Ho | Wind-up machine, launches Mario on contact | Cannot be defeated; use its throw to reach high areas. |

### Bosses

| Boss | Course | Mechanic |
|------|--------|----------|
| King Bob-omb | Bob-omb Battlefield | Grab from behind, throw 3 times. Must throw on the summit. |
| Whomp King | Whomp's Fortress | Dodge fall, ground-pound back. 3 hits. |
| Big Bully | Lethal Lava Land | Push into lava with attacks. |
| Big Boo | Big Boo's Haunt | Face away to lure it close, turn and punch. 3 hits. |
| Eyerok | Shifting Sand Land | Punch the eye on each hand when exposed. |
| Wiggler | Tiny-Huge Island | Jump on 3 times (gets angrier each time). |
| Bowser | 3 Bowser stages | See detailed Bowser fight section below. |

### Bowser Fight Detail

All three Bowser fights take place on a circular floating arena with spiked bombs placed around the edge.

**Core Mechanic — Grab, Spin, Throw:**
1. **Approach:** Get behind Bowser. He turns to face Mario, so circle-strafe or bait his charge attack to get past him.
2. **Grab tail:** Press B when behind Bowser and near his tail. Mario grabs on and enters the spin state.
3. **Spin:** Rotate the stick in circles. Mario swings Bowser around, building angular momentum. Faster rotation = farther throw. Visual: Bowser spins wider as speed builds.
4. **Aim and release:** Release B to throw Bowser in the direction he's currently facing. The goal is to hit one of the spiked bombs on the arena edge.
5. **Hit:** If Bowser hits a bomb, he takes damage and the bomb explodes. If he misses (lands on the arena or falls off and jumps back), he returns to his attack pattern.

**Bowser's Attacks:**
- **Fire breath:** Bowser shoots a stream of fire in an arc toward Mario. Jump over or run to the side.
- **Charge:** Bowser runs directly at Mario. Sidestep or jump over. After the charge he pauses briefly (best window to get behind him).
- **Ground pound shockwave (fights 2 & 3):** Bowser jumps and slams the arena, creating a shockwave ring that expands outward. Jump to avoid it.
- **Fire rain (fight 3 only):** Bowser breathes fire into the sky, which falls back as scattered fireballs across the arena. Watch shadows for safe spots.

**Per-Fight Differences:**

| Fight | Hits | Arena | Bowser Behavior |
|-------|------|-------|-----------------|
| Dark World | 1 | Full circle, 5 bombs | Fire breath and charge only. Slow movement. |
| Fire Sea | 1 | Full circle, 5 bombs | Adds ground pound shockwave. Faster movement. Arena tilts after Bowser takes damage. |
| Sky | 3 | Full circle → star shape | All attacks including fire rain. After hit 2, outer edge crumbles away forming a smaller star-shaped arena. Fewer bombs remain. Third throw requires more spin momentum to reach the remaining bombs. |

**Rewards:**
- Fight 1: Basement key.
- Fight 2: Second floor key.
- Fight 3: Grand Star — game completion.

## Object Interactions

| Object | Interaction |
|--------|-------------|
| Yellow coin | Walk through to collect (1 coin) |
| Red coin | Walk through to collect (2 coins); 8 per course, collecting all 8 spawns a Power Star |
| Blue coin | Walk through to collect (5 coins); often hidden or triggered by specific actions |
| Coin ring (circle of coins) | Pass through center or collect individually |
| 1-Up Mushroom | Chase and touch (moves away from Mario) |
| Star | Walk into to collect; triggers exit from course |
| Corkbox / Breakable box | Punch, ground-pound, or dive to break; may contain coins or 1-Up |
| Metal box | Only breakable with Metal Cap |
| Sign | Stand near and press B to read |
| NPC (Toad, Bob-omb Buddy) | Stand near and press B to talk |
| Cannon | Talk to Bob-omb Buddy to unlock; enter cannon hole, aim and launch Mario |
| Warp Pipe | Stand on to teleport |
| Door | Walk into to enter |
| Star Door | Requires star count to open |
| Pole / Tree | Jump onto to grab; climb with stick; jump off with A |
| Spinning Heart | Run through to restore health |
| Koopa Shell | Ride like a surfboard after knocking off a Koopa; fast movement, damages enemies on contact |
| Cap Block (red/green/blue) | Punch to activate; dispenses corresponding cap (only after switch pressed in secret stage) |
| Teleport warp | Invisible spots in certain courses; standing on one warps to a paired location |

## Level Data Format

Each course is defined by a set of data files in the resources directory:

### Geometry

- **Visual mesh:** OBJ (or custom binary) file defining the renderable level geometry with vertex positions, normals, UVs, and material/color assignments.
- **Collision mesh:** Separate (or tagged) triangle mesh used for physics only. Each triangle has:
  - Surface type enum (normal, ice, lava, shallow quicksand, deep quicksand, wall-only).
  - Collision layer bits.
  - Pre-computed surface normal.
- The collision mesh can be a simplified version of the visual mesh (fewer triangles in areas the player can't reach).

### Object Placement

A per-course data file (JSON, custom binary, or hardcoded structs during early development) listing all placed objects:

```
{
  "course_id": 1,
  "objects": [
    { "type": "goomba", "pos": [10, 0, 5], "patrol": [[10,0,5], [20,0,5]] },
    { "type": "coin_yellow", "pos": [15, 2, 8] },
    { "type": "star", "id": 3, "pos": [50, 20, 10], "condition": "defeat_king_bobomb" },
    { "type": "cannon", "pos": [5, 0, 30], "aim_dir": [0.5, 0.7, 0] },
    { "type": "warp_painting", "pos": [0, 3, -10], "target_course": 2 }
  ]
}
```

Each object entry defines:
- **type** — enemy, collectible, trigger, NPC, or interactable.
- **pos** — world-space spawn position.
- **Optional fields:** patrol path points, detection radius override, linked star ID, condition for appearing (e.g., only on certain star selections), target destination (for warps/doors).

### Per-Star Variations

Some objects only appear when a specific star is selected at the star select screen. The object placement data supports a `"star_filter": [1, 3]` field — the object only spawns if the player selected star 1 or 3. Objects with no star filter spawn always.

### Course Metadata

Per-course metadata (name, star names, sky color, music track, coin target, etc.) stored alongside the object data or in a master course list file.

## HUD Layout

All HUD elements rendered via gl2d + glui in orthographic projection over the 3D scene.

```
┌──────────────────────────────────────────────────┐
│ [Power Meter]                    [Star] x 12     │
│                                  [Coin] x 47     │
│                                  [Lives] x 4     │
│                                                  │
│                                                  │
│                                                  │
│                                                  │
│                                                  │
│                                                  │
│                                                  │
│                     [Course Name / Star Name]     │
└──────────────────────────────────────────────────┘
```

| Element | Position | Description |
|---------|----------|-------------|
| Power Meter | Top-left | 8-segment pie chart. Fills/drains with health. Flashes red at 2 or fewer segments. |
| Star Counter | Top-right | Star icon + total collected count. |
| Coin Counter | Top-right, below stars | Coin icon + current course coin count. |
| Lives Counter | Top-right, below coins | Mario icon + lives remaining. Only shown briefly on course entry or life change. |
| Course/Star Name | Bottom-center | Displays on course entry for ~3 seconds, then fades. |
| Action prompt | Bottom-center | Context-sensitive text when near signs/NPCs ("Press B to read"). |
| Pause overlay | Full screen | Semi-transparent dark overlay with pause menu (Resume / Exit Course) centered. |

## Menu Flow

### 1. Title Screen

- Background: 3D scene of Peach's Castle exterior (or a static image of it during early development).
- Game logo displayed top-center.
- "Press Start" prompt at bottom-center, pulsing or blinking.
- Any button press transitions to the File Select screen.
- If idle for ~30 seconds, plays a short demo/attract sequence (Mario running through a course). Optional — can skip for early builds.

### 2. File Select

- Four save file slots displayed as cards/panels.
- Each slot shows: file name (File A/B/C/D), star count, and a small Mario icon indicating progress.
- Empty slots show "New Game".
- Controls: up/down to select slot, A to confirm, B to go back to title.
- A secondary option to copy or delete a save file (accessible via a button prompt at the bottom).
- Selecting a populated file loads directly into the castle hub. Selecting an empty slot starts the intro sequence (camera flyover of the castle, Peach's letter, then places Mario outside the castle).

### 3. Star Select

- Shown when Mario jumps into a course painting.
- Displays the course name at the top and a numbered list of the 6 star mission names.
- Already-collected stars show a filled star icon; uncollected show an empty outline.
- Controls: up/down to highlight a star, A to confirm and enter the course.
- The selected star number is passed to the level loader for per-star object placement filtering.

### 4. Pause Menu

- Triggered by pressing Start during gameplay.
- Game freezes (all simulation stops).
- Semi-transparent dark overlay covering the 3D scene.
- Centered panel showing:
  - Course name and current star count.
  - "Continue" option (resume gameplay).
  - "Exit Course" option (return to castle hub, keep collected stars, lose current coin count).
- Controls: up/down to select, A to confirm, Start to resume (same as Continue).

### 5. Game Over

- Shown when lives reach 0.
- Displays "Game Over" text with Mario's face / sad animation.
- Two options: "Continue" (return to file select, lives reset to 4, keep all collected stars) or "Quit" (return to title screen).

### 6. Ending

- Triggered after defeating Bowser in the Sky (fight 3).
- Cutscene: Bowser is defeated, Peach is rescued, cake scene.
- Returns to the castle hub afterward. Player can continue collecting remaining stars.
- Collecting all 120 stars unlocks a bonus: Yoshi appears on the castle rooftop, gives 99 lives and a unique message.

## Audio

Sound design using raudio (already integrated). All assets are original or royalty-free — no ripped Nintendo audio.

### Music Tracks

Looping background tracks. Can be OGG or WAV format.

| Track | Where it plays | Mood/Style |
|-------|---------------|------------|
| Title Screen | Title screen, idle | Grand, inviting |
| File Select | File select menu | Light, cheerful |
| Castle Hub (inside) | Inside Peach's Castle | Calm, echoing |
| Castle Hub (outside) | Castle grounds / courtyard | Bright, open |
| Generic Overworld | Grassy/outdoor courses (Bob-omb Battlefield, etc.) | Upbeat, adventurous |
| Underground/Cave | Hazy Maze Cave, indoor sections | Mysterious, echoey |
| Snow/Ice | Cool, Cool Mountain, Snowman's Land | Crisp, playful |
| Desert | Shifting Sand Land | Warm, rhythmic |
| Water/Beach | Jolly Roger Bay, Dire, Dire Docks | Relaxed, flowing |
| Lava/Fire | Lethal Lava Land, Bowser stages | Tense, driving |
| Haunted | Big Boo's Haunt | Eerie, minor key |
| Sky/Heights | Rainbow Ride, Tall, Tall Mountain | Airy, soaring |
| Slide | Secret slide courses | Fast, fun |
| Boss Fight | Course bosses (King Bob-omb, Whomp King, etc.) | Intense, short loop |
| Bowser Fight | All 3 Bowser arenas | Heavy, dramatic |
| Underwater | While submerged | Muffled/filtered version of current course track, or dedicated calm track |

### Jingles (Non-Looping)

Short one-shot musical stings, ~2-5 seconds each.

| Jingle | Trigger |
|--------|---------|
| Star Collect | Collecting a Power Star |
| Key Collect | Receiving a key from Bowser |
| 1-Up | Collecting a 1-Up mushroom or earning a life |
| Star Select Confirm | Confirming star choice and entering a course |
| Course Clear | Exiting a course after collecting a star |
| Game Over | Lives reach 0 |
| Bowser Defeated | Bowser takes final hit |
| Ending/Credits | After defeating final Bowser |

### Sound Effects

All SFX are short one-shot samples (WAV recommended for low latency).

**Mario — Movement:**
| SFX | Trigger |
|-----|---------|
| Footstep (normal) | Each step while walking/running on solid ground |
| Footstep (sand) | Steps on sand/desert surfaces |
| Footstep (snow) | Steps on snow/ice surfaces |
| Footstep (metal) | Steps on metal/stone surfaces |
| Jump | Single/double/triple jump launch |
| Double Jump | Higher-pitched jump variant |
| Triple Jump | Highest-pitched jump + "wahoo" feel |
| Land (soft) | Landing from short height |
| Land (hard) | Landing from significant height / ground pound impact |
| Long Jump | Launching a long jump |
| Backflip | Launching a backflip |
| Wall Jump | Kicking off a wall |
| Ground Pound (whoosh) | Initiating ground pound in air |
| Ground Pound (impact) | Hitting the ground |
| Dive | Forward dive launch |
| Slide | Belly slide / slide kick loop |
| Ledge Grab | Catching a ledge |
| Climb | Shimming up a pole/tree (short repeating) |
| Swim Stroke | Each A-press while swimming |
| Splash (enter) | Entering water |
| Splash (exit) | Jumping out of water |
| Crawl | Crawling movement |

**Mario — Combat:**
| SFX | Trigger |
|-----|---------|
| Punch | Each hit of the punch combo |
| Kick | Final hit of combo / jump kick |
| Hit Enemy | Attack connecting with an enemy |
| Grab | Picking up an object or enemy |
| Throw | Throwing a held object |

**Mario — Damage:**
| SFX | Trigger |
|-----|---------|
| Hurt (small) | Taking 1 segment of damage |
| Hurt (big) | Taking 2-3 segments / lava / fall damage |
| Burned | Contact with fire/lava |
| Knockback | Being launched backward |
| Death | Losing a life |
| Drowning | Running out of air underwater |

**Collectibles:**
| SFX | Trigger |
|-----|---------|
| Coin | Collecting any coin |
| Red Coin | Collecting a red coin (distinct chime) |
| Blue Coin | Collecting a blue coin |
| 1-Up Mushroom | Collecting a 1-Up |
| Star Appear | A Power Star spawning / materializing |
| Star Collect | Touching a Power Star (plays before jingle) |
| Health Restore | Spinning Heart restoring health |
| Cap Collect | Picking up a Wing/Metal/Vanish cap |
| Cap Expire | Cap power-up wearing off (warning beeps before expiry) |

**Enemies:**
| SFX | Trigger |
|-----|---------|
| Goomba Squish | Goomba defeated by jump |
| Bob-omb Fuse | Bob-omb fuse burning (looping while chasing) |
| Bob-omb Explode | Bob-omb detonating |
| Boo Laugh | Boo approaching from behind |
| Boo Defeat | Boo hit and disappearing |
| Bowser Roar | Bowser intro / taking damage |
| Bowser Fire | Bowser breathing fire |
| Bowser Stomp | Bowser ground pound shockwave |
| Bowser Throw | Releasing Bowser during spin-throw |
| Bowser Hit Bomb | Bowser hitting a spiked bomb |
| Enemy Defeat (generic) | Default enemy death for enemies without a unique SFX |
| Thwomp Slam | Thwomp hitting the ground |
| Chain Chomp Bark | Chain Chomp lunge |
| Chain Chomp Free | Freeing Chain Chomp from post |

**Environment:**
| SFX | Trigger |
|-----|---------|
| Door Open | Opening a regular door |
| Star Door Open | Opening a star door (grander sound) |
| Cannon Launch | Being shot from a cannon |
| Warp Pipe | Entering/exiting a warp pipe |
| Painting Enter | Jumping into a course painting (watery ripple) |
| Switch Press | Pressing a cap switch |
| Block Break | Destroying a breakable box |
| Metal Block Break | Destroying a metal box with Metal Cap |
| Moving Platform | Looping hum/clank of a moving platform |
| Lava Bubble | Ambient lava bubbling |
| Wind | Ambient wind in exposed areas |
| Waterfall | Ambient waterfall loop |

**UI:**
| SFX | Trigger |
|-----|---------|
| Menu Select | Moving cursor between options |
| Menu Confirm | Confirming a selection (A press) |
| Menu Back | Going back (B press) |
| Pause | Opening pause menu |
| Unpause | Closing pause menu |
| Text Appear | Text printing on-screen (NPC dialog, sign) |

### Voice Clips (Optional)

Can be omitted entirely for early builds. If included, use original recordings — not Nintendo samples.

| Clip | When |
|------|------|
| "Here we go!" | Starting a course / new game |
| "Wahoo!" | Triple jump |
| "Haha!" | Double jump / successful action |
| "Yah!" / "Wah!" / "Hoo!" | Single jump variants (randomly pick one) |
| Effort grunt | Punching, throwing, climbing |
| Pain grunt (small) | Small damage |
| Pain yelp (big) | Big damage, lava, fall |
| "Mama mia!" | Death |
| Drowning gasp | Running out of air |

### Implementation Notes

- raudio supports WAV, OGG, MP3, and FLAC. Use OGG for music (smaller files), WAV for SFX (no decode latency).
- Music crossfades when transitioning between areas (~1 second fade).
- Underwater filter: when Mario is submerged, apply a low-pass filter or swap to a muffled version of the current track.
- Cap expiry warning: play a repeating beep during the last 5 seconds of a cap power-up.
- Footstep SFX are driven by animation events or a timer based on movement speed (faster = more frequent).

## Save System

- Game saves when Mario collects a star and exits a course.
- Four save file slots.
- Save data tracks: stars collected (per course), star total, keys obtained, cap switches pressed.

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

## References

- [Super Mario 64 — StrategyWiki: Controls](https://strategywiki.org/wiki/Super_Mario_64/Controls)
- [Super Mario 64 — StrategyWiki: Enemies](https://strategywiki.org/wiki/Super_Mario_64/Enemies)
- [Super Mario 64 — Super Mario Wiki](https://www.mariowiki.com/Super_Mario_64)
- [SM64 Move List and Guide — GameFAQs](https://gamefaqs.gamespot.com/n64/198848-super-mario-64/faqs/3324)
- [SM64 Bestiary — GameFAQs](https://gamefaqs.gamespot.com/n64/198848-super-mario-64/faqs/54401)
- [RTA Movement Guide — Ukikipedia](https://ukikipedia.net/wiki/RTA_Guide/Movement_Guide)
- [Camera — Ukikipedia](https://ukikipedia.net/wiki/Camera)
- [TAS Game Resources: SM64 — TASVideos](https://tasvideos.org/GameResources/N64/SuperMario64)
- [SM64 Cap Power-Ups — Game Rant](https://gamerant.com/super-mario-64-unlock-every-cap/)
- [Metal Cap — Super Mario Wiki](https://www.mariowiki.com/Metal_Cap)
- [Vanish Cap — Super Mario Wiki](https://www.mariowiki.com/Vanish_Cap)
- [Bowser in the Sky — Super Mario Wiki](https://www.mariowiki.com/Bowser_in_the_Sky)
- [SM64 120 Stars Guide — Mario Mayhem](https://www.mariomayhem.com/consoles/walkthroughs/mario_64_120_stars_guide.php)
- [SM64 Walkthrough — Mario Party Legacy](https://mariopartylegacy.com/guides/super-mario-64-walkthrough-and-stars)
- [Super Mario 64 — Wikipedia](https://en.wikipedia.org/wiki/Super_Mario_64)
