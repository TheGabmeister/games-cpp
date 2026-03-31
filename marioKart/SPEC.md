# SPEC.md

## Overview

This project aims to recreate the feel of Nintendo 64-era kart racing inside the current C++17/OpenGL repo.

The goal is not a pixel-perfect Mario Kart 64 remake. The goal is to capture the gameplay loop that makes that style of game fun:

- arcade kart handling
- drifting and short boosts
- lap-based racing with checkpoints
- item-driven combat and recovery
- readable track flow
- simple but competitive AI racers

The presentation will be intentionally minimal:

- primitive shapes only
- flat colors only
- no textured environments
- no character models
- no music or sound effects

This should feel like a strong mechanics-first prototype, not like a content-complete commercial remake.

## Design Pillars

- Mechanics first. If a feature does not improve driving feel, race flow, or item decisions, it is lower priority.
- Readability over authenticity. The game state should be easy to understand even if it looks much simpler than Nintendo 64 Mario Kart.
- Small, testable steps. Every milestone should leave the project in a runnable, verifiable state.
- Original implementation. Recreate the feel of the mechanics, not Nintendo assets, names, UI, models, audio, or exact content.
- Simple tech choices. Prefer straightforward C++17 structs and subsystem-style code over a complicated engine architecture.

## Hard Constraints

- Language: C++17
- Repo structure: game code belongs in `src/gameLayer/` and `include/gameLayer/`
- Rendering: OpenGL with primitive geometry and flat colors
- Audio: none
- Core mode: single-player race against AI
- Scope target: one solid vertical slice before adding extra content

## Legal and Content Boundary

This project should be Mario Kart 64-inspired, not a direct asset or content copy.

- Do not use Nintendo textures, models, music, voice clips, UI art, or ripped course data.
- Do not depend on exact character rosters or copyrighted presentation.
- It is acceptable to target similar mechanics, race pacing, item roles, and camera feel.
- Use original placeholder names for tracks, racers, and item identifiers in code where possible.

## Technical Direction

### World Model

- The game is 3D, not top-down 2D.
- Use a simple ground-plane convention:
  - `X` and `Z` form the track plane
  - `Y` is height
- Initial implementation should assume mostly flat tracks.
- Height changes, jumps, and drops are optional later additions, not part of the first playable milestone.

### Simulation Model

- Use a fixed simulation step of 60 Hz for gameplay systems if practical.
- Rendering can remain once per frame, but kart movement, AI, items, and race progression should be updated in stable steps.
- Clamp unexpected large frame times so the race does not explode after pauses or breakpoints.
- Favor deterministic-enough behavior for tuning and debugging, even if full determinism is not a hard requirement.

### Units and Scale

- Pick one consistent unit system early and keep it everywhere.
- Recommended default:
  - 1 world unit = 1 meter-like gameplay unit
  - kart length roughly 1.8 to 2.2 units
  - track width roughly 8 to 14 units for the first course
- Tune feel first; realism is not required.

### Rendering Rules

- Use flat colors only.
- No texture sampling is required for gameplay.
- Primitive shapes may include:
  - boxes
  - quads
  - lines
  - prisms
  - cylinders or low-sided approximations if useful
- Lighting should stay simple:
  - unlit flat shading is acceptable
  - a tiny amount of directional shading is acceptable if it improves depth readability

## Input and Controls

Keyboard-first controls should be supported from the start.

Recommended default mapping:

- `Up`: accelerate
- `Down`: brake / reverse
- `Left`: steer left
- `Right`: steer right
- `Space`: hop / drift
- `Enter`: use item
- `Tab`: toggle debug overlay later if needed
- `Escape`: pause or quit flow later

Controller support is optional in the first implementation, even though platform support exists.

Control rules:

- controls should be responsive and readable with only keyboard input
- no advanced input buffering is required at first
- remapping is out of scope for the first version

## Core Game Scope

### Camera and Presentation

- Third-person chase camera behind the player kart
- Camera should prioritize readability over cinematic motion
- Mild spring smoothing is acceptable
- Camera should point slightly ahead of the kart to make corners readable
- Field of view should feel wide enough for racing without excessive distortion

Primitive-shape visual language:

- player kart: bright distinct color
- AI karts: clearly different flat colors
- road: dark neutral color
- off-road: dull contrasting color
- walls: high-contrast border color
- item boxes: rotating or pulsing simple prisms/cubes
- hazards: distinct silhouettes and colors

HUD should remain minimal:

- lap number
- current position
- held item
- countdown / finish state
- optional speed or boost indicator

Text rendering is optional. If text is inconvenient early, use simple bars, digits, or debug labels.

### Race Structure

- Single race mode first
- One course loaded at a time
- Default race length: 3 laps
- Countdown before player control unlocks
- Race ends when the player completes the final valid lap
- Ranking is based on lap count, checkpoint progress, and distance through the current segment
- Start grid should support at least 4 racers in the first full race
- Long-term target for the core prototype: 4 to 8 racers total

### Kart Handling

Handling should be arcade-like, readable, and forgiving.

Required behavior:

- forward acceleration to capped top speed
- braking and reverse
- steering sensitivity that depends on speed
- drag when not accelerating
- some lateral slip
- simple collision response against walls
- simple collision nudges between karts
- off-road slowdown

Explicit simplifications:

- no full rigid-body simulation
- no tire simulation
- no realistic suspension model
- no need for exact Mario Kart 64 physics formulas

Recommended state per kart:

- position
- velocity
- facing angle / forward vector
- current speed
- drift state
- boost timer
- grounded state
- lap/checkpoint progress
- held item
- recovery timers
- AI control state when applicable

### Drift and Boost Mechanics

Drifting is one of the most important feel targets.

Required drift behavior:

- hop input initiates drift mode
- steering behavior changes during drift
- lateral slip increases during drift
- the kart can face slightly away from its velocity direction
- mini-turbo is earned after sustaining a good drift long enough

Important feel goals:

- starting a drift feels like a commitment
- drifting gives a cornering advantage if used well
- mini-turbo reward is obvious and short

Recommended simplifications:

- one drift button
- no need to exactly copy Mario Kart 64's left-right wiggle technique
- one mini-turbo tier is enough for the first version

Boost sources:

- mini-turbo
- boost pads
- mushroom item

Boost behavior:

- temporary acceleration and/or top-speed increase
- reduced penalty from off-road while boosted
- boost logic should be centralized so all boost sources use the same timer/state where practical

### Track and World Rules

Tracks should be designed as gameplay spaces first.

Required course features:

- clear drivable road
- walls or barrier volumes where needed
- off-road zones
- finish line
- ordered checkpoints for lap validation

Optional later features:

- ramps
- shortcuts
- moving hazards
- jumps
- split paths

Initial track target:

- one simple loop track
- mostly flat
- wide turns
- enough variety to test drifting, items, AI, and lap logic

Second track target:

- one more technical circuit
- tighter corners
- more meaningful shortcut decisions

### Track Representation

To reduce complexity, track data should start with a simple format.

Recommended first representation:

- ordered centerline waypoints
- per-segment width values
- checkpoint markers tied to the waypoint chain
- simple wall volumes placed by hand where needed
- optional surface zones for off-road and boost pads

This is easier to test than building a full course editor or importing meshes.

### Checkpoints, Ranking, and Recovery

Checkpoint logic must prevent shortcut abuse.

Rules:

- laps only count after checkpoints are crossed in order
- racers store current checkpoint index
- racer ranking is computed from:
  - lap number
  - checkpoint index
  - distance toward the next checkpoint
- wrong-way warning should trigger when the player is moving strongly opposite the current checkpoint direction for a sustained short period

Recovery rules:

- store a recent safe pose after valid track progress
- if the kart leaves valid bounds or becomes unrecoverable:
  - freeze briefly
  - respawn at the last safe pose
  - align to track direction
  - clear extreme velocities

### Items

Items are required for the target feel, but they should be introduced gradually.

General item rules:

- each kart may hold one item at a time
- item boxes only grant an item if the kart does not already hold one
- item usage should have a short lockout/cooldown to avoid accidental double use
- item behavior must be readable without audio

Initial item box behavior:

- player drives through an item pickup zone
- receives one random item from a constrained table
- first implementation can use uniform random selection
- later tuning may use position-weighted item tables

Initial item set:

- Mushroom equivalent: short speed boost
- Banana equivalent: dropped hazard
- Green shell equivalent: straight projectile

Second-wave item set:

- Red shell equivalent: simple homing projectile
- Invincibility/star equivalent: optional later addition
- Lightning-like global attack: stretch only if the rest of the race already feels great

Item design goals:

- each item has a clear role
- each item has readable movement or placement
- each item can be tested in isolation
- item balance should favor fun and clarity over authenticity

### AI Opponents

AI should start simple and become convincing through tuning, not complexity.

Phase 1 AI:

- follows waypoint targets
- basic throttle and steering toward the next target
- simple catch-up speed tuning

Phase 2 AI:

- better corner speed management
- basic item use
- simple local avoidance
- stuck detection and recovery

Recommended AI rules:

- give each AI racer a target line offset so they do not all overlap perfectly
- allow small driving mistakes; perfect AI can feel unfair
- rubber-banding should be gentle and mostly used to keep the pack coherent

### Collision Rules

Collision should stay simple and robust.

Recommended first-pass collision shapes:

- karts: circle, cylinder, or simple capsule in the track plane
- walls: axis-aligned or oriented boxes
- item pickups and hazards: circles/spheres or simple boxes

Collision goals:

- no tunneling at normal gameplay speeds
- wall hits feel readable, not chaotic
- kart-to-kart collisions create nudges, not pile-up instability
- hazards cause clear penalties without fully stopping race flow

## HUD and Debugging

The game should expose enough information to tune mechanics quickly.

Required HUD/debug data during development:

- speed
- drift state
- boost timer
- lap number
- checkpoint index
- race position
- held item
- countdown / race state

Debug views that would be useful later:

- checkpoint markers
- AI target points
- collision bounds
- respawn points

## Saving and Persistence

Persistent save data is not required for the first playable version.

- race progress can reset on each launch
- best laps, settings, and unlocks are out of scope initially
- do not design core gameplay systems around save/load dependencies

## Architecture Guidance

Recommended state containers:

- `GameState`
- `RaceState`
- `TrackState`
- `Kart`
- `ItemEntity`
- `HazardEntity`
- `Checkpoint`
- `CameraState`
- `HudState`

Recommended coding style:

- plain structs for state
- update systems as free functions or small managers
- minimal inheritance
- explicit update order
- avoid overengineering around ECS for the first version

Suggested top-level update order:

1. collect input
2. advance fixed-step simulation loop
3. update race state and countdown
4. update player intent
5. update AI intents
6. update kart movement and drift state
7. resolve collisions and surface effects
8. update items and hazards
9. update checkpoints, laps, and ranking
10. update camera
11. render world
12. render HUD/debug overlays

## Success Criteria

The project succeeds when:

- driving feels responsive and fun with keyboard controls
- the player can finish a full 3-lap race against AI
- drifting and mini-turbo meaningfully affect lap times
- at least three distinct items affect race outcomes
- ranking, lap counting, and checkpoint validation work reliably
- the race remains readable using only primitive shapes and flat colors

## Risks and Mitigations

### Risk: Scope creep from "full Mario Kart clone" expectations

Mitigation:

- keep the first target to one strong track and one strong race loop
- delay extra items, extra tracks, and extra modes until the base race is fun

### Risk: Primitive-only visuals make the game hard to read

Mitigation:

- use strong color coding
- keep track boundaries exaggerated
- add debug markers early
- prioritize camera readability over flashy motion

### Risk: Drift system becomes hard to tune

Mitigation:

- use a small number of drift parameters
- centralize boost logic
- add visible debug values for drift state and boost timers

### Risk: AI appears lifeless or unfair

Mitigation:

- start with stable waypoint following
- introduce mistakes and offset lines
- tune pacing after lap and ranking logic are correct

## Development Roadmap

The order below is arranged so every phase leaves the game in an easy-to-test state.

### Phase 0: Technical Base

#### Task 0.1: Replace the sample rectangle with a gameplay scaffold

- create persistent game state structures for race, track, karts, camera, and render data
- keep the app rendering a clear background and a visible placeholder world
- add a lightweight debug state readout path if useful

Test:

- app launches cleanly
- no crashes on startup/shutdown
- the frame clear and basic world render path are ready for gameplay code

#### Task 0.2: Add a simple primitive renderer

- support colored quads, boxes, lines, and markers
- support a camera view/projection setup
- keep the rendering API intentionally small

Test:

- can draw a ground plane, a few boxes, and markers from a controllable camera
- window resize still works

#### Task 0.3: Add fixed-step simulation support

- implement a 60 Hz gameplay step or equivalent stable update loop
- clamp large frame-time spikes

Test:

- movement remains stable across varying frame rates
- pausing in a debugger does not permanently destabilize the race

### Phase 1: Driving Prototype

#### Task 1.1: Add a flat test arena and one controllable kart

- create a simple ground plane
- represent the kart as a colored block shape
- add acceleration, braking, reverse, and steering

Test:

- player can drive around the arena with keyboard input
- movement is stable and frame-rate independent

#### Task 1.2: Add chase camera behavior

- camera follows behind the kart
- camera points toward the kart and track ahead
- add only enough smoothing to improve readability

Test:

- camera stays readable during turns
- player can judge heading and speed from the view

#### Task 1.3: Add speed tuning, drag, and steering curves

- tune acceleration, top speed, reverse speed, and coasting drag
- reduce steering sharpness at high speed if needed

Test:

- kart feels controllable at low and medium speeds
- letting go of acceleration slows the kart naturally

### Phase 2: Track and Race Progress

#### Task 2.1: Build the first test track

- replace the arena with a simple loop course
- add visible road, boundaries, and off-road regions
- add the finish line and checkpoint markers

Test:

- player can complete a clean loop
- course boundaries are easy to read

#### Task 2.2: Add wall collision and off-road slowdown

- prevent the kart from leaving the course through walls
- reduce speed and acceleration on off-road surfaces

Test:

- wall hits cause a readable bounce or stop
- driving off-road is clearly slower than staying on the road

#### Task 2.3: Add checkpoints, lap counting, and wrong-way warning

- require ordered checkpoint progression
- detect wrong-way travel and expose a warning state

Test:

- crossing the finish line only counts after valid checkpoint order
- driving backward does not falsely advance laps

#### Task 2.4: Add countdown and finish flow

- start countdown before control unlocks
- end the race when the configured lap target is reached

Test:

- race cannot start early
- finishing the last lap ends the race cleanly

### Phase 3: Drift and Boost Feel

#### Task 3.1: Add hop and drift state

- separate normal turning from drift turning
- add hop-to-drift behavior
- increase lateral slip while drifting

Test:

- player can start drift intentionally
- drifting clearly changes cornering behavior

#### Task 3.2: Add mini-turbo reward

- track valid drift duration or drift quality
- grant a short boost after a successful drift

Test:

- clean drifts produce an obvious speed gain
- failed or too-short drifts do not trigger the boost

#### Task 3.3: Add boost pads and unified boost tuning

- place boost pads on the track
- reuse the same boost-state logic for pads and items where practical
- tune wall-hit recovery and boost handling

Test:

- boost pads reliably increase speed
- boost state interacts sensibly with turning and off-road

### Phase 4: AI Racers and Positioning

#### Task 4.1: Add waypoint-based AI karts

- spawn several AI racers
- drive them around the course using waypoints and target offsets

Test:

- AI completes laps without constant failure
- AI stays mostly on the intended path

#### Task 4.2: Add ranking logic

- rank racers by lap, checkpoint, and progress toward the next checkpoint
- expose player position in the HUD/debug layer

Test:

- overtaking changes race position correctly
- rankings remain stable around the finish line

#### Task 4.3: Add AI recovery and pacing

- recover stuck AI
- tune corner speed and lightweight rubber-banding

Test:

- AI rarely stalls permanently
- races stay competitive enough to be interesting

### Phase 5: Items

#### Task 5.1: Add item boxes and held-item state

- place item boxes on the course
- grant one held item on pickup

Test:

- driving through a box grants exactly one item
- the held item is visible in the HUD/debug layer

#### Task 5.2: Add the mushroom-equivalent boost item

- trigger a short boost on use

Test:

- player can collect and activate the item
- the boost clearly affects race outcomes

#### Task 5.3: Add the banana-equivalent dropped hazard

- drop a hazard behind the kart
- hitting it causes slowdown or spinout

Test:

- player and AI can both trigger the hazard
- recovery is quick enough to keep race flow intact

#### Task 5.4: Add the green-shell-equivalent projectile

- fire a straight-moving projectile
- allow collisions with walls and karts

Test:

- projectile travels predictably
- projectile can hit racers in front

#### Task 5.5: Add the red-shell-equivalent projectile

- fire a simple homing projectile at a valid target ahead

Test:

- homing is readable and limited
- projectile affects target racers without feeling magical

### Phase 6: Race Polish

#### Task 6.1: Improve HUD and race readability

- show lap, place, countdown, finish state, and held item
- keep the visual language primitive and uncluttered

Test:

- a new player can understand race state without guessing

#### Task 6.2: Add out-of-bounds recovery

- detect invalid course exit or fall state
- respawn at the last safe checkpoint pose

Test:

- player cannot soft-lock by leaving the course
- recovery is fast and consistent

#### Task 6.3: Tune the full 3-lap race loop

- tune kart handling
- tune AI speed and item frequency
- tune track width, wall placement, and shortcut value

Test:

- a full 3-lap race is playable and fun
- the main systems reinforce each other instead of fighting each other

## Stretch Goals

- a second, more technical track
- more items
- lightweight local ghost replay
- better primitive-shape animation on karts and items
- simple cup/championship flow across multiple races
- local split-screen only after the single-player race is strong

## Recommended Implementation Order Summary

If we want the shortest path to a fun prototype, the order should be:

1. gameplay scaffold
2. primitive renderer
3. fixed-step simulation
4. one controllable kart
5. chase camera
6. simple loop track
7. walls and off-road
8. checkpoints and laps
9. countdown and finish
10. drift and mini-turbo
11. AI racers
12. rankings
13. item boxes
14. mushroom-equivalent boost item
15. banana-equivalent hazard
16. shell-equivalent projectile
17. HUD and race polish

## Milestone Definitions

The first milestone worth calling playable is:

- one track
- one player kart
- chase camera
- wall collision
- off-road slowdown
- lap counting
- countdown and finish flow

The first milestone worth calling Mario Kart-like is:

- everything in the playable milestone
- drifting
- mini-turbo
- AI racers
- race positions
- at least three meaningful items
