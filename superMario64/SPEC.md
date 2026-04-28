# Super Mario 64 — Game Specification

A simplified recreation of Super Mario 64 focused on core gameplay mechanics with simple 3D graphics. Not a pixel-perfect clone — the goal is to capture the feel and fun of the original through faithful movement, physics, and level design principles.

## Rendering Approach

- **3D world:** Direct OpenGL 3.3 Core for the game world. Levels built from basic geometry (cubes, ramps, cylinders). Characters and enemies are simple meshes or billboarded sprites. No complex texturing or lighting required — flat/vertex colors and basic diffuse shading are sufficient.
- **2D overlays:** gl2d + glui for all HUD elements (health meter, coin counter, star count, lives) and game menus (title screen, file select, star select, pause menu).

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

- **Default mode:** Third-person camera orbits behind Mario at a set distance and height.
- **Player control:** Right stick (or C-buttons equivalent) rotates the camera horizontally around Mario. Vertical angle has limited range.
- **Auto-adjust:** Camera slowly re-centers behind Mario's movement direction when not manually controlled.
- **Zoom modes:** Close and far toggle (or smooth zoom).
- **Special modes:** Camera may switch behavior in tight corridors, boss arenas, or underwater.
- **First-person look:** When stationary, a button press enters first-person look mode (look around only, no movement).

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
| Bowser | 3 Bowser stages | Grab tail, spin with stick rotation, throw into spiked bombs on arena edge. Requires 1/1/3 hits. Final fight: arena crumbles after 2 hits. |

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

## Menu Flow

1. **Title Screen** — Press Start.
2. **File Select** — Four save file slots. Choose a file to play or delete.
3. **Star Select** — Shown when entering a course painting. Displays the 6 star mission names and which ones are already collected.
4. **Pause Menu** — Resume or Exit Course. Shows star count and current course name.
5. **Game Over** — Shown at 0 lives. Continue from file select or quit.
6. **Ending** — Plays after defeating final Bowser. Collecting all 120 stars unlocks a bonus (rooftop meeting with Yoshi, extra lives).

## Audio

Simple sound design using raudio (already integrated):

- **SFX:** Jump, coin collect, punch/kick, star collect, damage taken, ground pound, swimming, enemy defeat, boss hit, door open, menu select.
- **Music:** Per-course background track, hub world theme, boss theme, Bowser fight theme, star collect jingle, game over, title screen, file select.
- **Ambient:** Water splashing, lava bubbling, wind in outdoor courses.
- **Voice clips (optional):** Mario's "Wahoo!", "Here we go!", damage grunts. Can be omitted for simplicity.

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
