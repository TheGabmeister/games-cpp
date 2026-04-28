# Super Mario 64 — Game Specification

A simplified recreation of Super Mario 64 focused on core gameplay mechanics with simple 3D graphics. Not a pixel-perfect clone — the goal is to capture the feel and fun of the original through faithful movement, physics, and level design principles.

## Rendering Approach

Simple 3D using OpenGL (replacing the current gl2d 2D renderer). Levels built from basic geometry (cubes, ramps, cylinders). Characters and enemies are simple meshes or billboarded sprites. No complex texturing or lighting required — flat/vertex colors and basic diffuse shading are sufficient.

## Core Movement

Mario's movement is analog — walking speed is proportional to stick deflection. Full tilt = run.

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
| Swim (underwater) | A to stroke, stick to steer | Free 3D movement; limited air meter |

## Physics

### Gravity and Momentum

- Gravity is constant while airborne (no variable gravity zones in base game).
- Mario preserves horizontal momentum into jumps. Faster run = longer jump arc.
- Landing on slopes: Mario slides down steep slopes. Gentle slopes allow normal movement.
- Ice surfaces: reduced friction, Mario slides and has delayed turning response.

### Knockback

- Taking damage knocks Mario backward with a fixed impulse.
- Falling from great heights causes fall damage (takes 3 health) and brief stun.
- Lava causes immediate knockback + 3 damage and launches Mario upward.

### Water

- Mario floats on the surface by default.
- Diving underwater starts an air meter (8 segments, shared with health display or separate).
- Air depletes over time; replenished by collecting coins or surfacing.
- At zero air, health drains rapidly.
- Metal Cap allows walking on the bottom with infinite air.

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
- Red coin: restores 2 segments.
- Spinning Heart: restores health while Mario runs through it.
- Entering water (surface): restores health over time.

### Lives

- Mario starts with 4 lives.
- 1-Up mushrooms grant an extra life.
- Collecting 100 coins in a course grants an extra life.
- Collecting 50 coins in a course grants an extra life.
- Game Over at 0 lives — restart from title screen / last save.

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
- Courses accessed by jumping into paintings (or equivalent portals).
- Doors require star counts to open (1-star door, 3-star door, 8-star door, etc.).
- Key locations unlock as player progresses (basement after first Bowser, upper floors later).

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
| 8 | Basement (after Bowser in the Dark World) |
| 12 | Upper courses door |
| 30 | Second floor (after Bowser in the Fire Sea) |
| 50 | Third floor door |
| 70 | Bowser in the Sky (endless stair bypass) |

**Total: 120 Power Stars** (105 from main courses + 15 from secret courses/castle).

## Cap Power-Ups

Activated by hitting colored cap blocks (must first press corresponding switch in a secret stage).

| Cap | Color | Duration | Effect |
|-----|-------|----------|--------|
| Wing Cap | Red | 60 seconds | Triple jump or cannon launch enters flight mode. Stick controls pitch/yaw. |
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
| Boo | Invisible when Mario faces it, approaches from behind | Ground pound or punch while facing away. |
| Bully | Charges at Mario to push him off platforms | Push back by attacking; knock into lava/off edge. No direct damage to Mario. |
| Chain Chomp | Lunges at Mario while chained to post | Cannot be defeated normally; ground-pound post to free it (star reward). |
| Piranha Plant | Sleeps, bites if Mario gets too close | Punch or jump on while awake; don't approach while sleeping (it stays asleep). |
| Amp | Electric orb, orbits a fixed path | Cannot be defeated; avoid contact (electric damage). |
| Thwomp | Slams down when Mario walks beneath | Cannot be defeated; time passage between slams. |
| Whomp | Falls forward to crush Mario | Dodge the fall, ground-pound its back. |

### Bosses

| Boss | Course | Mechanic |
|------|--------|----------|
| King Bob-omb | Bob-omb Battlefield | Grab from behind, throw 3 times. Must throw on the summit. |
| Whomp King | Whomp's Fortress | Dodge fall, ground-pound back. 3 hits. |
| Big Bully | Lethal Lava Land | Push into lava with attacks. |
| Big Boo | Big Boo's Haunt | Ground-pound 3 times (face away to make it approach). |
| Eyerok | Shifting Sand Land | Punch the eye on each hand when exposed. |
| Wiggler | Tiny-Huge Island | Jump on 3 times (gets angrier each time). |
| Bowser | 3 Bowser stages | Grab tail, spin with stick rotation, throw into spiked bombs on arena edge. Requires 1/1/3 hits. Final fight: arena crumbles after 2 hits. |

## Object Interactions

| Object | Interaction |
|--------|-------------|
| Yellow/Red/Blue coin | Walk through to collect |
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
