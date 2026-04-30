#include "mario.h"
#include "renderer.h"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <algorithm>

const char *marioStateName(MarioState s)
{
	switch (s)
	{
	case MarioState::IDLE: return "IDLE";
	case MarioState::WALKING: return "WALKING";
	case MarioState::RUNNING: return "RUNNING";
	case MarioState::SKIDDING: return "SKIDDING";
	case MarioState::SINGLE_JUMP: return "SINGLE_JUMP";
	case MarioState::DOUBLE_JUMP: return "DOUBLE_JUMP";
	case MarioState::TRIPLE_JUMP: return "TRIPLE_JUMP";
	case MarioState::LONG_JUMP: return "LONG_JUMP";
	case MarioState::BACKFLIP: return "BACKFLIP";
	case MarioState::SIDE_SOMERSAULT: return "SIDE_SOMERSAULT";
	case MarioState::FREEFALL: return "FREEFALL";
	case MarioState::GROUND_POUND_SPIN: return "GROUND_POUND_SPIN";
	case MarioState::GROUND_POUND_FALL: return "GROUND_POUND_FALL";
	case MarioState::GROUND_POUND_LAND: return "GROUND_POUND_LAND";
	case MarioState::CROUCHING: return "CROUCHING";
	case MarioState::CRAWLING: return "CRAWLING";
	case MarioState::PUNCH_1: return "PUNCH_1";
	case MarioState::PUNCH_2: return "PUNCH_2";
	case MarioState::PUNCH_3_KICK: return "PUNCH_3_KICK";
	case MarioState::JUMP_KICK: return "JUMP_KICK";
	case MarioState::DIVE: return "DIVE";
	case MarioState::BELLY_SLIDE: return "BELLY_SLIDE";
	case MarioState::SLIDE_KICK: return "SLIDE_KICK";
	case MarioState::WALL_SLIDE: return "WALL_SLIDE";
	case MarioState::WALL_JUMP: return "WALL_JUMP";
	case MarioState::LEDGE_HANG: return "LEDGE_HANG";
	case MarioState::LEDGE_CLIMB: return "LEDGE_CLIMB";
	case MarioState::POLE_GRAB: return "POLE_GRAB";
	case MarioState::POLE_CLIMB: return "POLE_CLIMB";
	case MarioState::CARRY_IDLE: return "CARRY_IDLE";
	case MarioState::CARRY_WALK: return "CARRY_WALK";
	case MarioState::THROW: return "THROW";
	case MarioState::DROP: return "DROP";
	case MarioState::LANDING: return "LANDING";
	case MarioState::KNOCKBACK: return "KNOCKBACK";
	case MarioState::DEATH: return "DEATH";
	case MarioState::SWIMMING_SURFACE: return "SWIMMING_SURFACE";
	case MarioState::SWIMMING_UNDERWATER: return "SWIMMING_UNDERWATER";
	case MarioState::COUNT: break;
	}
	return "UNKNOWN";
}

// ---- Utility ----

static float angleDiff(float a, float b)
{
	float d = fmod(a - b + 540.f, 360.f) - 180.f;
	return d;
}

static float lerpAngle(float from, float to, float maxStep)
{
	float diff = angleDiff(to, from);
	if (std::abs(diff) < maxStep) return to;
	return from + (diff > 0 ? maxStep : -maxStep);
}

// ---- Animation Setup ----

static const char *stateToClipName(MarioState s)
{
	switch (s)
	{
	case MarioState::IDLE: return "idle";
	case MarioState::WALKING: return "walk";
	case MarioState::RUNNING: return "run";
	case MarioState::SKIDDING: return "skid";
	case MarioState::SINGLE_JUMP: return "single_jump";
	case MarioState::DOUBLE_JUMP: return "double_jump";
	case MarioState::TRIPLE_JUMP: return "triple_jump";
	case MarioState::LONG_JUMP: return "long_jump";
	case MarioState::BACKFLIP: return "backflip";
	case MarioState::SIDE_SOMERSAULT: return "side_somersault";
	case MarioState::FREEFALL: return "freefall";
	case MarioState::GROUND_POUND_SPIN: return "ground_pound_spin";
	case MarioState::GROUND_POUND_FALL: return "ground_pound_fall";
	case MarioState::GROUND_POUND_LAND: return "ground_pound_land";
	case MarioState::CROUCHING: return "crouch";
	case MarioState::CRAWLING: return "crawl";
	case MarioState::PUNCH_1: return "punch_1";
	case MarioState::PUNCH_2: return "punch_2";
	case MarioState::PUNCH_3_KICK: return "punch_3_kick";
	case MarioState::JUMP_KICK: return "jump_kick";
	case MarioState::DIVE: return "dive";
	case MarioState::BELLY_SLIDE: return "belly_slide";
	case MarioState::SLIDE_KICK: return "slide_kick";
	case MarioState::WALL_SLIDE: return "wall_slide";
	case MarioState::WALL_JUMP: return "wall_jump";
	case MarioState::LEDGE_HANG: return "ledge_hang";
	case MarioState::LEDGE_CLIMB: return "ledge_climb";
	case MarioState::POLE_GRAB: return "pole_climb";
	case MarioState::POLE_CLIMB: return "pole_climb";
	case MarioState::CARRY_IDLE: return "carry_idle";
	case MarioState::CARRY_WALK: return "carry_walk";
	case MarioState::THROW: return "throw";
	case MarioState::DROP: return "drop";
	case MarioState::LANDING: return "landing";
	case MarioState::KNOCKBACK: return "knockback";
	case MarioState::DEATH: return "death";
	case MarioState::SWIMMING_SURFACE: return "swim_surface";
	case MarioState::SWIMMING_UNDERWATER: return "swim_underwater";
	case MarioState::COUNT: break;
	}
	return "idle";
}

void Mario::setAnimClips(const SkinnedModel &model)
{
	for (int i = 0; i < (int)MarioState::COUNT; i++)
	{
		const char *name = stateToClipName((MarioState)i);
		auto it = model.clipNameToIndex.find(name);
		if (it != model.clipNameToIndex.end())
			clipForState[i] = it->second;
		else
			clipForState[i] = 0;
	}
	hasAnimClips = true;
	playClip(animState, clipForState[(int)MarioState::IDLE], 0.f);
}

// ---- Helpers ----

float Mario::getHorizontalSpeed() const
{
	return glm::length(glm::vec2(velocity.x, velocity.z));
}

glm::vec3 Mario::getFacingForward() const
{
	float angle = glm::radians(facingAngle);
	return glm::normalize(glm::vec3(sinf(angle), 0.f, cosf(angle)));
}

GroundResult Mario::checkGround(const CollisionWorld *world, Phase5World *phase5World,
	float previousY, float maxAbove, float maxBelow, int *platformIndex) const
{
	GroundResult r;
	if (platformIndex)
		*platformIndex = -1;

	if (world && world->isLoaded())
	{
		CollisionHit hit;
		if (queryGround(*world, position, COLLISION_RADIUS, maxAbove, maxBelow, hit))
		{
			r.hit = true;
			r.groundY = hit.y;
			r.normal = hit.normal;
			r.surface = hit.surface;
			r.slopeClass = hit.slopeClass;
		}
	}

	if (phase5World)
	{
		CollisionHit platformHit;
		int hitObject = -1;
		if (queryPhase5PlatformGround(*phase5World, position, previousY, COLLISION_RADIUS, maxAbove, maxBelow, platformHit, hitObject) &&
			(!r.hit || platformHit.y > r.groundY))
		{
			r.hit = true;
			r.groundY = platformHit.y;
			r.normal = platformHit.normal;
			r.surface = platformHit.surface;
			r.slopeClass = platformHit.slopeClass;
			if (platformIndex)
				*platformIndex = hitObject;
		}
	}

	if ((!world || !world->isLoaded()) && !r.hit && position.y <= maxAbove)
	{
		r.hit = true;
		r.groundY = 0.f;
		r.normal = {0.f, 1.f, 0.f};
	}
	return r;
}

glm::vec3 Mario::inputToWorldDir(const GameInput &input, const glm::vec3 &cameraForward) const
{
	if (input.moveStrength < 0.01f) return {0, 0, 0};

	glm::vec3 camFwd = glm::normalize(glm::vec3(cameraForward.x, 0.f, cameraForward.z));
	glm::vec3 camRight = glm::normalize(glm::cross(camFwd, glm::vec3(0, 1, 0)));

	glm::vec3 worldDir = camFwd * input.moveDir.y + camRight * input.moveDir.x;
	float len = glm::length(worldDir);
	if (len > 0.001f) worldDir /= len;
	return worldDir;
}

// ---- State Entry ----

void Mario::enterState(MarioState newState)
{
	state = newState;

	if (hasAnimClips)
		playClip(animState, clipForState[(int)newState], 0.1f);

	switch (newState)
	{
	case MarioState::SKIDDING:
		skidTimer = 0.15f;
		break;
	case MarioState::LANDING:
		landingTimer = 0.08f;
		break;
	case MarioState::GROUND_POUND_SPIN:
		stateTimer = 0.15f;
		velocity = {0.f, 0.f, 0.f};
		break;
	case MarioState::GROUND_POUND_LAND:
		stateTimer = 0.25f;
		velocity = {0.f, 0.f, 0.f};
		break;
	case MarioState::PUNCH_1:
		stateTimer = 0.2f;
		punchComboStep = 1;
		comboTimer = 0.f;
		break;
	case MarioState::PUNCH_2:
		stateTimer = 0.2f;
		punchComboStep = 2;
		comboTimer = 0.f;
		break;
	case MarioState::PUNCH_3_KICK:
		stateTimer = 0.25f;
		punchComboStep = 0;
		comboTimer = 0.f;
		break;
	case MarioState::BELLY_SLIDE:
		break;
	case MarioState::SLIDE_KICK:
		break;
	case MarioState::LEDGE_HANG:
		velocity = {};
		break;
	case MarioState::LEDGE_CLIMB:
		velocity = {};
		ledgeTimer = 0.25f;
		break;
	case MarioState::POLE_GRAB:
	case MarioState::POLE_CLIMB:
		velocity = {};
		break;
	case MarioState::THROW:
	case MarioState::DROP:
		actionTimer = 0.2f;
		break;
	case MarioState::KNOCKBACK:
		stateTimer = 0.5f;
		break;
	case MarioState::DEATH:
		deathTimer = DEATH_DURATION;
		break;
	case MarioState::SWIMMING_SURFACE:
		velocity.y = 0.f;
		onGround = false;
		swimPitch = 0.f;
		break;
	case MarioState::SWIMMING_UNDERWATER:
		onGround = false;
		break;
	default:
		break;
	}
}

// ---- Physics Helpers ----

void Mario::applyGravity(const GameInput &input, float dt)
{
	float g = GRAVITY;

	bool canVaryHeight = (state == MarioState::SINGLE_JUMP ||
	                      state == MarioState::DOUBLE_JUMP ||
	                      state == MarioState::BACKFLIP ||
	                      state == MarioState::SIDE_SOMERSAULT ||
	                      state == MarioState::WALL_JUMP);
	if (canVaryHeight && input.jumpHeld && velocity.y > 0.f)
		g = GRAVITY_HELD;

	if (state == MarioState::GROUND_POUND_FALL)
		g *= GROUND_POUND_GRAVITY_MULT;

	velocity.y -= g * dt;
	if (velocity.y < -TERMINAL_VELOCITY)
		velocity.y = -TERMINAL_VELOCITY;
}

void Mario::applyGroundMovement(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	glm::vec3 wishDir = inputToWorldDir(input, cameraForward);
	float targetSpeed = input.moveStrength * RUN_SPEED;
	float accelScale = (onGround && groundSurface == SurfaceType::Ice) ? 0.35f : 1.f;
	float decelScale = (onGround && groundSurface == SurfaceType::Ice) ? 0.12f : 1.f;
	float turnScale = (onGround && groundSurface == SurfaceType::Ice) ? 0.35f : 1.f;

	glm::vec2 hVel(velocity.x, velocity.z);
	float hSpeed = glm::length(hVel);

	if (input.moveStrength > 0.01f)
	{
		float wishAngle = glm::degrees(atan2(wishDir.x, wishDir.z));

		if (hSpeed > SKID_THRESHOLD)
		{
			float velAngle = glm::degrees(atan2(velocity.x, velocity.z));
			float diff = std::abs(angleDiff(wishAngle, velAngle));
			if (diff > 120.f)
			{
				enterState(MarioState::SKIDDING);
			}
		}

		if (state != MarioState::SKIDDING)
		{
			glm::vec2 wishVel = glm::vec2(wishDir.x, wishDir.z) * targetSpeed;
			glm::vec2 accel = (wishVel - hVel);
			float accelLen = glm::length(accel);
			if (accelLen > 0.001f)
			{
				float maxAccel = GROUND_ACCEL * accelScale * dt;
				if (accelLen > maxAccel)
					accel = accel / accelLen * maxAccel;
				hVel += accel;
			}

			facingAngle = lerpAngle(facingAngle, wishAngle, TURN_SPEED * turnScale * dt);

			float speed = glm::length(hVel);
			if (speed > WALK_SPEED * 0.5f && speed <= WALK_SPEED)
				state = MarioState::WALKING;
			else if (speed > WALK_SPEED)
				state = MarioState::RUNNING;
			else
				state = MarioState::IDLE;
		}
	}
	else
	{
		float decel = GROUND_DECEL * decelScale * dt;
		if (hSpeed > decel)
			hVel *= (hSpeed - decel) / hSpeed;
		else
			hVel = {0, 0};

		state = (glm::length(hVel) > 0.1f) ? MarioState::WALKING : MarioState::IDLE;
	}

	velocity.x = hVel.x;
	velocity.z = hVel.y;
}

void Mario::applyAirMovement(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	glm::vec3 wishDir = inputToWorldDir(input, cameraForward);
	if (input.moveStrength > 0.01f)
	{
		glm::vec2 hVel(velocity.x, velocity.z);
		glm::vec2 wishVel = glm::vec2(wishDir.x, wishDir.z) * RUN_SPEED;
		glm::vec2 accel = (wishVel - hVel);
		float accelLen = glm::length(accel);
		if (accelLen > 0.001f)
		{
			float maxAccel = AIR_ACCEL * dt;
			if (accelLen > maxAccel)
				accel = accel / accelLen * maxAccel;
			hVel += accel;
		}
		velocity.x = hVel.x;
		velocity.z = hVel.y;

		float wishAngle = glm::degrees(atan2(wishDir.x, wishDir.z));
		facingAngle = lerpAngle(facingAngle, wishAngle, TURN_SPEED * 0.5f * dt);
	}
}

// ---- Input Buffers ----

void Mario::updateInputBuffers(const GameInput &input, float dt)
{
	if (input.jump)
		jumpBufferTimer = JUMP_BUFFER_WINDOW;
	else
		jumpBufferTimer = std::max(0.f, jumpBufferTimer - dt);

	if (onGround)
		coyoteTimer = COYOTE_TIME;
	else
		coyoteTimer = std::max(0.f, coyoteTimer - dt);
	wallContactTimer = std::max(0.f, wallContactTimer - dt);

	jumpChainTimer += dt;

	if (punchComboStep > 0 && state != MarioState::PUNCH_1 &&
	    state != MarioState::PUNCH_2 && state != MarioState::PUNCH_3_KICK)
	{
		comboTimer += dt;
		if (comboTimer > COMBO_WINDOW)
		{
			punchComboStep = 0;
			comboTimer = 0.f;
		}
	}
}

// ---- Jump Dispatcher ----

void Mario::tryGroundJump(const GameInput &input, const glm::vec3 &cameraForward)
{
	float hSpeed = getHorizontalSpeed();

	// Long jump: crouch + jump while moving
	if (input.crouchHeld && hSpeed > 4.f)
	{
		velocity.y = LONG_JUMP_VERTICAL;
		float angle = glm::radians(facingAngle);
		velocity.x = sinf(angle) * LONG_JUMP_HORIZONTAL;
		velocity.z = cosf(angle) * LONG_JUMP_HORIZONTAL;
		onGround = false;
		jumpChainCount = 0;
		enterState(MarioState::LONG_JUMP);
		return;
	}

	// Backflip: crouch + jump while stationary/slow
	if (input.crouchHeld)
	{
		velocity.y = BACKFLIP_VELOCITY;
		velocity.x *= 0.1f;
		velocity.z *= 0.1f;
		onGround = false;
		jumpChainCount = 0;
		enterState(MarioState::BACKFLIP);
		return;
	}

	// Side somersault: jump during skid
	if (state == MarioState::SKIDDING)
	{
		velocity.y = SIDE_SOMERSAULT_VELOCITY;
		onGround = false;
		jumpChainCount = 0;
		enterState(MarioState::SIDE_SOMERSAULT);
		return;
	}

	// Triple jump: chain count 2, fast enough, within window
	if (jumpChainCount == 2 && hSpeed > TRIPLE_JUMP_SPEED_THRESHOLD &&
	    jumpChainTimer < JUMP_CHAIN_WINDOW)
	{
		velocity.y = TRIPLE_JUMP_VELOCITY;
		onGround = false;
		jumpChainCount = 0;
		enterState(MarioState::TRIPLE_JUMP);
		return;
	}

	// Double jump: chain count 1, within window
	if (jumpChainCount == 1 && jumpChainTimer < JUMP_CHAIN_WINDOW)
	{
		velocity.y = DOUBLE_JUMP_VELOCITY;
		onGround = false;
		jumpChainCount = 2;
		enterState(MarioState::DOUBLE_JUMP);
		return;
	}

	// Single jump (default)
	velocity.y = JUMP_VELOCITY;
	onGround = false;
	currentPlatform = -1;
	jumpChainCount = 1;
	enterState(MarioState::SINGLE_JUMP);
}

// ---- World Collision ----

static bool isAirborneState(MarioState state)
{
	return state == MarioState::SINGLE_JUMP || state == MarioState::DOUBLE_JUMP ||
		state == MarioState::TRIPLE_JUMP || state == MarioState::LONG_JUMP ||
		state == MarioState::BACKFLIP || state == MarioState::SIDE_SOMERSAULT ||
		state == MarioState::FREEFALL || state == MarioState::GROUND_POUND_SPIN ||
		state == MarioState::GROUND_POUND_FALL || state == MarioState::JUMP_KICK ||
		state == MarioState::DIVE || state == MarioState::SLIDE_KICK ||
		state == MarioState::WALL_SLIDE || state == MarioState::WALL_JUMP ||
		state == MarioState::KNOCKBACK || state == MarioState::DEATH;
}

void Mario::resolveCollision(const CollisionWorld *world, Phase5World *phase5World, float dt, const glm::vec3 &previousPosition)
{
	if (state == MarioState::LEDGE_HANG || state == MarioState::LEDGE_CLIMB ||
		state == MarioState::POLE_GRAB || state == MarioState::POLE_CLIMB)
		return;

	if (world && world->isLoaded())
	{
		glm::vec3 hitWallNormal;
		resolveHorizontalCollisions(*world, position, velocity, COLLISION_RADIUS, HEIGHT, STEP_HEIGHT, &hitWallNormal);
		if (glm::length(hitWallNormal) > 0.001f)
		{
			lastWallNormal = hitWallNormal;
			wallContactTimer = 0.12f;
		}

		if (velocity.y > 0.f)
		{
			CollisionHit ceiling;
			if (queryCeiling(*world, position, COLLISION_RADIUS, HEIGHT, ceiling))
			{
				position.y = ceiling.y - HEIGHT - 0.02f;
				velocity.y = 0.f;
				if (state == MarioState::SINGLE_JUMP || state == MarioState::DOUBLE_JUMP ||
					state == MarioState::TRIPLE_JUMP || state == MarioState::BACKFLIP ||
					state == MarioState::SIDE_SOMERSAULT || state == MarioState::LONG_JUMP)
				{
					enterState(MarioState::FREEFALL);
				}
			}
		}
	}

	float maxAbove = STEP_HEIGHT;
	float maxBelow = 0.65f;
	if (!onGround)
	{
		maxAbove = 0.08f;
		maxBelow = std::max(0.25f, std::max(0.f, previousPosition.y - position.y) + 0.2f);
	}

	int platformIndex = -1;
	GroundResult ground = checkGround(world, phase5World, previousPosition.y, maxAbove, maxBelow, &platformIndex);

	if (ground.hit && velocity.y <= 0.f)
	{
		position.y = ground.groundY;
		velocity.y = 0.f;
		groundNormal = ground.normal;
		groundSurface = ground.surface;
		groundSlope = ground.slopeClass;
		currentPlatform = platformIndex;
		if (phase5World && platformIndex >= 0 && platformIndex < (int)phase5World->objects.size())
		{
			Phase5Object &platform = phase5World->objects[platformIndex];
			glm::vec3 platformDelta = platform.position - platform.previousPosition;
			position += platformDelta;
			if (platform.type == Phase5ObjectType::TiltingPlatform &&
				(std::abs(platform.tiltX) > 18.f || std::abs(platform.tiltZ) > 18.f))
			{
				velocity.x += -platform.tiltZ * 0.03f;
				velocity.z += platform.tiltX * 0.03f;
			}
		}

		if (ground.slopeClass == SlopeClass::Steep)
		{
			glm::vec3 downSlope = glm::vec3(0.f, -1.f, 0.f) -
				ground.normal * glm::dot(glm::vec3(0.f, -1.f, 0.f), ground.normal);
			if (glm::length(downSlope) > 0.001f)
			{
				downSlope = glm::normalize(downSlope);
				float uphill = glm::dot(glm::vec3(velocity.x, 0.f, velocity.z), -downSlope);
				if (uphill < TRIPLE_JUMP_SPEED_THRESHOLD)
				{
					velocity.x += downSlope.x * 18.f * dt;
					velocity.z += downSlope.z * 18.f * dt;
				}
			}
		}

		if (!onGround)
		{
			// Fall damage check
			float fallDistance = airborneMaxY - position.y;
			if (fallDistance > FALL_DAMAGE_THRESHOLD && state != MarioState::DEATH)
			{
				onGround = true;
				takeDamage(FALL_DAMAGE_SEGMENTS, position);
				if (!isDead())
				{
					enterState(MarioState::GROUND_POUND_LAND);
					stateTimer = 0.4f;
				}
				return;
			}

			// Just landed — jumpChainCount was already set in tryGroundJump

			jumpChainTimer = 0.f;

			// Special landing states
			if (state == MarioState::GROUND_POUND_FALL)
			{
				onGround = true;
				groundPoundImpact = true;
				enterState(MarioState::GROUND_POUND_LAND);
				return;
			}
			if (state == MarioState::DIVE)
			{
				onGround = true;
				enterState(MarioState::BELLY_SLIDE);
				return;
			}

			// Check jump buffer — chained jump
			if (jumpBufferTimer > 0.f)
			{
				onGround = true;
				// updateLanding consumes this on the next fixed step, where camera-relative
				// jump variants have the input context they need.
			}

			onGround = true;
			enterState(MarioState::LANDING);
			return;
		}

		onGround = true;
	}
	else if (!ground.hit)
	{
		groundNormal = {0.f, 1.f, 0.f};
		groundSurface = SurfaceType::Normal;
		groundSlope = SlopeClass::Walkable;
		currentPlatform = -1;

		if (onGround)
		{
			// Just left ground — start coyote timer (already set in updateInputBuffers)
			onGround = false;
		}

		if (!isAirborneState(state))
		{
			enterState(MarioState::FREEFALL);
		}
	}
}

// ---- State Handlers ----

static void decelerateHorizontal(glm::vec3 &velocity, float decel)
{
	glm::vec2 hVel(velocity.x, velocity.z);
	float hSpeed = glm::length(hVel);
	if (hSpeed > decel)
		hVel *= (hSpeed - decel) / hSpeed;
	else
		hVel = {0, 0};
	velocity.x = hVel.x;
	velocity.z = hVel.y;
}

void Mario::updateIdle(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	if (input.crouchHeld)
	{
		enterState(MarioState::CROUCHING);
		return;
	}
	if (input.attack)
	{
		if (punchComboStep == 0)
			enterState(MarioState::PUNCH_1);
		else if (punchComboStep == 1)
			enterState(MarioState::PUNCH_2);
		else if (punchComboStep == 2)
			enterState(MarioState::PUNCH_3_KICK);
		return;
	}
	if (jumpBufferTimer > 0.f)
	{
		jumpBufferTimer = 0.f;
		tryGroundJump(input, cameraForward);
		return;
	}

	applyGroundMovement(input, dt, cameraForward);
}

void Mario::updateWalking(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	if (input.crouchHeld)
	{
		enterState(MarioState::CROUCHING);
		return;
	}
	if (input.attack)
	{
		if (punchComboStep == 0)
			enterState(MarioState::PUNCH_1);
		else if (punchComboStep == 1)
			enterState(MarioState::PUNCH_2);
		else if (punchComboStep == 2)
			enterState(MarioState::PUNCH_3_KICK);
		return;
	}
	if (jumpBufferTimer > 0.f)
	{
		jumpBufferTimer = 0.f;
		tryGroundJump(input, cameraForward);
		return;
	}

	applyGroundMovement(input, dt, cameraForward);
}

void Mario::updateRunning(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	if (input.crouchHeld)
	{
		enterState(MarioState::CROUCHING);
		return;
	}
	if (input.attack)
	{
		if (punchComboStep == 0)
			enterState(MarioState::PUNCH_1);
		else if (punchComboStep == 1)
			enterState(MarioState::PUNCH_2);
		else if (punchComboStep == 2)
			enterState(MarioState::PUNCH_3_KICK);
		return;
	}
	if (jumpBufferTimer > 0.f)
	{
		jumpBufferTimer = 0.f;
		tryGroundJump(input, cameraForward);
		return;
	}

	applyGroundMovement(input, dt, cameraForward);
}

void Mario::updateSkidding(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	skidTimer -= dt;
	decelerateHorizontal(velocity, GROUND_DECEL * 1.5f * dt);

	// Jump during skid → side somersault
	if (jumpBufferTimer > 0.f)
	{
		jumpBufferTimer = 0.f;
		tryGroundJump(input, cameraForward);
		return;
	}

	if (skidTimer <= 0.f)
		state = MarioState::IDLE;
}

void Mario::updateCrouching(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	decelerateHorizontal(velocity, GROUND_DECEL * dt);

	if (!input.crouchHeld)
	{
		state = MarioState::IDLE;
		return;
	}
	if (jumpBufferTimer > 0.f)
	{
		jumpBufferTimer = 0.f;
		tryGroundJump(input, cameraForward);
		return;
	}
	if (input.moveStrength > 0.1f)
	{
		state = MarioState::CRAWLING;
		return;
	}
}

void Mario::updateCrawling(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	if (!input.crouchHeld)
	{
		state = MarioState::IDLE;
		return;
	}
	if (input.moveStrength < 0.1f)
	{
		state = MarioState::CROUCHING;
		return;
	}
	if (jumpBufferTimer > 0.f)
	{
		jumpBufferTimer = 0.f;
		tryGroundJump(input, cameraForward);
		return;
	}
	// Slide kick: attack while crawling
	if (input.attack && getHorizontalSpeed() > 1.f)
	{
		velocity.y = 2.f;
		float angle = glm::radians(facingAngle);
		velocity.x = sinf(angle) * SLIDE_KICK_SPEED;
		velocity.z = cosf(angle) * SLIDE_KICK_SPEED;
		onGround = false;
		enterState(MarioState::SLIDE_KICK);
		return;
	}

	glm::vec3 wishDir = inputToWorldDir(input, cameraForward);
	if (glm::length(wishDir) > 0.001f)
	{
		float wishAngle = glm::degrees(atan2(wishDir.x, wishDir.z));
		facingAngle = lerpAngle(facingAngle, wishAngle, TURN_SPEED * 0.5f * dt);
	}

	velocity.x = wishDir.x * CRAWL_SPEED * input.moveStrength;
	velocity.z = wishDir.z * CRAWL_SPEED * input.moveStrength;
}

void Mario::updateAirborne(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	// Ground pound: crouch while airborne (not from long jump, dive, or jump kick)
	if (input.crouch && state != MarioState::LONG_JUMP && state != MarioState::DIVE &&
	    state != MarioState::JUMP_KICK && state != MarioState::GROUND_POUND_SPIN &&
	    state != MarioState::GROUND_POUND_FALL)
	{
		enterState(MarioState::GROUND_POUND_SPIN);
		return;
	}

	// Dive / jump kick: attack while airborne
	if (input.attack && state != MarioState::DIVE && state != MarioState::JUMP_KICK)
	{
		float hSpeed = getHorizontalSpeed();
		if (hSpeed > WALK_SPEED)
		{
			float angle = glm::radians(facingAngle);
			velocity.x = sinf(angle) * DIVE_HORIZONTAL_SPEED;
			velocity.z = cosf(angle) * DIVE_HORIZONTAL_SPEED;
			if (velocity.y < 0.f) velocity.y = 0.f;
			enterState(MarioState::DIVE);
			return;
		}
		else
		{
			enterState(MarioState::JUMP_KICK);
			return;
		}
	}

	// Coyote jump: just fell off edge, jump buffer active
	if (coyoteTimer > 0.f && jumpBufferTimer > 0.f && state == MarioState::FREEFALL)
	{
		jumpBufferTimer = 0.f;
		coyoteTimer = 0.f;
		onGround = false;
		velocity.y = JUMP_VELOCITY;
		enterState(MarioState::SINGLE_JUMP);
		return;
	}

	applyAirMovement(input, dt, cameraForward);
	applyGravity(input, dt);

	// Transition ascending → freefall
	if ((state == MarioState::SINGLE_JUMP || state == MarioState::DOUBLE_JUMP ||
	     state == MarioState::TRIPLE_JUMP || state == MarioState::BACKFLIP ||
	     state == MarioState::SIDE_SOMERSAULT || state == MarioState::WALL_JUMP) && velocity.y <= 0.f)
	{
		enterState(MarioState::FREEFALL);
	}
}

void Mario::updateLongJump(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	// No air control during long jump
	applyGravity(input, dt);

	if (velocity.y <= 0.f)
		enterState(MarioState::FREEFALL);
}

void Mario::updateGroundPoundSpin(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	velocity = {0.f, 0.f, 0.f};
	stateTimer -= dt;
	if (stateTimer <= 0.f)
		enterState(MarioState::GROUND_POUND_FALL);
}

void Mario::updateGroundPoundFall(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	velocity.x = 0.f;
	velocity.z = 0.f;
	applyGravity(input, dt);
}

void Mario::updateTimedState(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	stateTimer -= dt;
	decelerateHorizontal(velocity, GROUND_DECEL * 0.5f * dt);

	// Allow queuing next combo hit during current punch
	if (state == MarioState::PUNCH_1 && input.attack && stateTimer < 0.1f)
	{
		enterState(MarioState::PUNCH_2);
		return;
	}
	if (state == MarioState::PUNCH_2 && input.attack && stateTimer < 0.1f)
	{
		enterState(MarioState::PUNCH_3_KICK);
		return;
	}

	if (stateTimer <= 0.f)
		state = MarioState::IDLE;
}

void Mario::updateBellySlide(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	decelerateHorizontal(velocity, GROUND_DECEL * 0.3f * dt);

	float hSpeed = getHorizontalSpeed();
	if (input.jump || input.moveStrength > 0.3f || hSpeed < 0.5f)
	{
		state = MarioState::IDLE;
		return;
	}
}

void Mario::updateSlideKick(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	applyGravity(input, dt);

	// Once on ground, decelerate like belly slide
	if (onGround)
	{
		decelerateHorizontal(velocity, GROUND_DECEL * 0.4f * dt);
		float hSpeed = getHorizontalSpeed();
		if (hSpeed < 0.5f)
		{
			state = MarioState::IDLE;
			return;
		}
	}
}

void Mario::updateWallSlide(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	if (jumpBufferTimer > 0.f && wallContactTimer > 0.f)
	{
		jumpBufferTimer = 0.f;
		velocity = lastWallNormal * WALL_JUMP_HORIZONTAL + glm::vec3(0.f, WALL_JUMP_VERTICAL, 0.f);
		float angle = glm::degrees(atan2(lastWallNormal.x, lastWallNormal.z));
		facingAngle = angle;
		onGround = false;
		enterState(MarioState::WALL_JUMP);
		return;
	}

	if (wallContactTimer <= 0.f)
	{
		enterState(MarioState::FREEFALL);
		return;
	}

	applyAirMovement(input, dt, cameraForward);
	applyGravity(input, dt);
	if (velocity.y < -5.f)
		velocity.y = -5.f;
}

void Mario::updateLedgeHang(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	position = ledgeTarget - lastWallNormal * COLLISION_RADIUS;
	velocity = {};
	onGround = false;

	if (input.crouchHeld || input.moveDir.y < -0.5f)
	{
		enterState(MarioState::FREEFALL);
		return;
	}

	if (input.jump || input.moveDir.y > 0.5f)
	{
		enterState(MarioState::LEDGE_CLIMB);
	}
}

void Mario::updateLedgeClimb(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	ledgeTimer -= dt;
	position = glm::mix(position, ledgeTarget + lastWallNormal * 0.25f, 1.f - std::exp(-12.f * dt));
	velocity = {};
	if (ledgeTimer <= 0.f)
	{
		position = ledgeTarget + lastWallNormal * 0.25f;
		onGround = true;
		enterState(MarioState::IDLE);
	}
}

void Mario::updatePole(const GameInput &input, float dt, const glm::vec3 &cameraForward, Phase5World *phase5World)
{
	if (!phase5World || activePole < 0 || activePole >= (int)phase5World->poles.size())
	{
		activePole = -1;
		enterState(MarioState::FREEFALL);
		return;
	}

	const Phase5Pole &pole = phase5World->poles[activePole];
	glm::vec2 fromPole(position.x - pole.base.x, position.z - pole.base.z);
	if (glm::length(fromPole) < 0.001f)
		fromPole = {0.f, -1.f};
	fromPole = glm::normalize(fromPole);

	position.x = pole.base.x + fromPole.x * (pole.radius + COLLISION_RADIUS);
	position.z = pole.base.z + fromPole.y * (pole.radius + COLLISION_RADIUS);
	position.y = std::clamp(position.y + input.moveDir.y * POLE_CLIMB_SPEED * dt, pole.base.y, pole.base.y + pole.height - HEIGHT * 0.5f);
	velocity = {};
	onGround = false;
	state = std::abs(input.moveDir.y) > 0.1f ? MarioState::POLE_CLIMB : MarioState::POLE_GRAB;

	if (input.crouchHeld)
	{
		activePole = -1;
		enterState(MarioState::FREEFALL);
		return;
	}

	if (input.jump)
	{
		glm::vec3 away(fromPole.x, 0.f, fromPole.y);
		velocity = away * 13.f + glm::vec3(0.f, 20.f, 0.f);
		activePole = -1;
		enterState(MarioState::SINGLE_JUMP);
	}
}

void Mario::updateCarry(const GameInput &input, float dt, const glm::vec3 &cameraForward, Phase5World *phase5World)
{
	actionTimer = std::max(0.f, actionTimer - dt);
	if (!phase5World || carriedObject < 0)
	{
		carriedObject = -1;
		enterState(MarioState::IDLE);
		return;
	}

	if (input.attack && actionTimer <= 0.f)
	{
		glm::vec3 forward = getFacingForward();
		throwCarriedObject(*phase5World, carriedObject, forward * 18.f + glm::vec3(0.f, 8.f, 0.f));
		carriedObject = -1;
		enterState(MarioState::THROW);
		return;
	}

	if (input.crouchHeld)
	{
		setCarriedObject(*phase5World, carriedObject, false);
		carriedObject = -1;
		enterState(MarioState::DROP);
		return;
	}

	applyGroundMovement(input, dt, cameraForward);
	state = getHorizontalSpeed() > 0.2f ? MarioState::CARRY_WALK : MarioState::CARRY_IDLE;
	moveCarriedObject(*phase5World, carriedObject, position + getFacingForward() * 0.85f + glm::vec3(0.f, 0.75f, 0.f));
}

void Mario::updateThrowDrop(const GameInput &input, float dt, const glm::vec3 &cameraForward, Phase5World *phase5World)
{
	actionTimer -= dt;
	decelerateHorizontal(velocity, GROUND_DECEL * dt);
	if (actionTimer <= 0.f)
		enterState(MarioState::IDLE);
}

void Mario::updateLanding(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	landingTimer -= dt;

	// Allow jump during landing for responsive chaining
	if (jumpBufferTimer > 0.f)
	{
		jumpBufferTimer = 0.f;
		tryGroundJump(input, cameraForward);
		return;
	}

	if (landingTimer <= 0.f)
		state = MarioState::IDLE;
}

void Mario::updateKnockback(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	applyGravity(input, dt);
	stateTimer -= dt;
	if (onGround && stateTimer <= 0.f)
		enterState(MarioState::IDLE);
}

void Mario::updateDeath(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	applyGravity(input, dt);
	deathTimer -= dt;
	if (deathTimer <= 0.f)
	{
		lives--;
		if (lives > 0)
			respawn();
	}
}

// ---- Health & Damage ----

void Mario::takeDamage(int segments, const glm::vec3 &sourcePos)
{
	if (isInvincible() || isDead())
		return;

	health -= segments;
	if (health < 0) health = 0;

	if (health <= 0)
	{
		die();
		return;
	}

	invincibilityTimer = INVINCIBILITY_DURATION;

	glm::vec3 knockDir = position - sourcePos;
	knockDir.y = 0.f;
	float len = glm::length(knockDir);
	if (len > 0.001f)
		knockDir /= len;
	else
		knockDir = -getFacingForward();

	velocity = knockDir * KNOCKBACK_SPEED + glm::vec3(0.f, 10.f, 0.f);
	onGround = false;
	carriedObject = -1;
	activePole = -1;
	enterState(MarioState::KNOCKBACK);
}

void Mario::heal(int segments)
{
	health = std::min(health + segments, maxHealth);
}

void Mario::collectCoin(int value)
{
	int prevCoins = coins;
	coins += value;
	heal(value);

	if (prevCoins < 50 && coins >= 50)
		lives++;
	if (prevCoins < 100 && coins >= 100)
		lives++;
}

void Mario::die()
{
	health = 0;
	velocity = {0.f, 18.f, 0.f};
	onGround = false;
	carriedObject = -1;
	activePole = -1;
	enterState(MarioState::DEATH);
}

bool Mario::isInvincible() const
{
	return invincibilityTimer > 0.f;
}

bool Mario::isDead() const
{
	return state == MarioState::DEATH;
}

bool Mario::isAttacking() const
{
	return state == MarioState::PUNCH_1 || state == MarioState::PUNCH_2 ||
		state == MarioState::PUNCH_3_KICK || state == MarioState::JUMP_KICK ||
		state == MarioState::DIVE || state == MarioState::SLIDE_KICK ||
		state == MarioState::GROUND_POUND_FALL;
}

bool Mario::isStomping() const
{
	return !onGround && velocity.y < 0.f &&
		(state == MarioState::SINGLE_JUMP || state == MarioState::DOUBLE_JUMP ||
		state == MarioState::TRIPLE_JUMP || state == MarioState::FREEFALL ||
		state == MarioState::GROUND_POUND_FALL);
}

void Mario::respawn()
{
	position = spawnPosition;
	velocity = {};
	health = maxHealth;
	invincibilityTimer = 0.f;
	deathTimer = 0.f;
	airborneMaxY = spawnPosition.y;
	onGround = true;
	carriedObject = -1;
	activePole = -1;
	currentPlatform = -1;
	currentWaterVolume = -1;
	airTimer = 0.f;
	swimPitch = 0.f;
	enterState(MarioState::IDLE);
}

// ---- Swimming ----

void Mario::enterWater(float surfaceY)
{
	preSwimHealth = health;
	airTimer = 0.f;
	jumpChainCount = 0;
	carriedObject = -1;
	activePole = -1;

	if (position.y >= surfaceY - HEIGHT * 0.5f)
		enterState(MarioState::SWIMMING_SURFACE);
	else
		enterState(MarioState::SWIMMING_UNDERWATER);
}

void Mario::exitWater()
{
	currentWaterVolume = -1;
	airTimer = 0.f;
	swimPitch = 0.f;
	health = std::max(health, preSwimHealth);
}

void Mario::updateSwimmingSurface(const GameInput &input, float dt, const glm::vec3 &cameraForward, Phase5World *phase5World)
{
	if (!phase5World || currentWaterVolume < 0 || currentWaterVolume >= (int)phase5World->waterVolumes.size())
	{
		exitWater();
		enterState(MarioState::FREEFALL);
		return;
	}

	const WaterVolume &wv = phase5World->waterVolumes[currentWaterVolume];
	position.y = wv.surfaceY;
	velocity.y = 0.f;

	// Check if still in water volume horizontally
	if (position.x < wv.minBounds.x || position.x > wv.maxBounds.x ||
		position.z < wv.minBounds.z || position.z > wv.maxBounds.z)
	{
		exitWater();
		enterState(MarioState::FREEFALL);
		return;
	}

	// Jump out of water
	if (input.jump)
	{
		velocity.y = SWIM_SURFACE_JUMP_VELOCITY;
		exitWater();
		onGround = false;
		enterState(MarioState::SINGLE_JUMP);
		return;
	}

	// Dive underwater
	if (input.crouch)
	{
		velocity.y = -5.f;
		position.y -= 0.5f;
		enterState(MarioState::SWIMMING_UNDERWATER);
		return;
	}

	// Horizontal movement
	glm::vec3 wishDir = inputToWorldDir(input, cameraForward);
	if (input.moveStrength > 0.01f)
	{
		float wishAngle = glm::degrees(atan2(wishDir.x, wishDir.z));
		facingAngle = lerpAngle(facingAngle, wishAngle, TURN_SPEED * 0.5f * dt);

		glm::vec2 hVel(velocity.x, velocity.z);
		glm::vec2 wishVel = glm::vec2(wishDir.x, wishDir.z) * SWIM_SURFACE_SPEED * input.moveStrength;
		glm::vec2 accel = wishVel - hVel;
		float accelLen = glm::length(accel);
		if (accelLen > 0.001f)
		{
			float maxAccel = GROUND_ACCEL * 0.5f * dt;
			if (accelLen > maxAccel)
				accel = accel / accelLen * maxAccel;
			hVel += accel;
		}
		velocity.x = hVel.x;
		velocity.z = hVel.y;
	}
	else
	{
		float decel = GROUND_DECEL * 0.5f * dt;
		glm::vec2 hVel(velocity.x, velocity.z);
		float hSpeed = glm::length(hVel);
		if (hSpeed > decel)
			hVel *= (hSpeed - decel) / hSpeed;
		else
			hVel = {0, 0};
		velocity.x = hVel.x;
		velocity.z = hVel.y;
	}

	// Paddle burst
	if (input.attack)
	{
		float angle = glm::radians(facingAngle);
		velocity.x = sinf(angle) * SWIM_PADDLE_BURST;
		velocity.z = cosf(angle) * SWIM_PADDLE_BURST;
	}
}

void Mario::updateSwimmingUnderwater(const GameInput &input, float dt, const glm::vec3 &cameraForward, Phase5World *phase5World)
{
	if (!phase5World || currentWaterVolume < 0 || currentWaterVolume >= (int)phase5World->waterVolumes.size())
	{
		exitWater();
		enterState(MarioState::FREEFALL);
		return;
	}

	const WaterVolume &wv = phase5World->waterVolumes[currentWaterVolume];

	// Surface check
	if (position.y >= wv.surfaceY)
	{
		position.y = wv.surfaceY;
		health = maxHealth;
		enterState(MarioState::SWIMMING_SURFACE);
		return;
	}

	// Ground check — walk out of shallow water
	if (onGround && position.y >= wv.surfaceY - HEIGHT)
	{
		exitWater();
		enterState(MarioState::IDLE);
		return;
	}

	// 3D swimming movement
	glm::vec3 camFwd = glm::normalize(glm::vec3(cameraForward.x, 0.f, cameraForward.z));
	glm::vec3 camRight = glm::normalize(glm::cross(camFwd, glm::vec3(0, 1, 0)));

	if (input.moveStrength > 0.01f)
	{
		// Pitch control via vertical stick
		swimPitch += input.moveDir.y * 120.f * dt;
		swimPitch = std::clamp(swimPitch, -60.f, 60.f);

		// Yaw from horizontal stick
		float wishAngle = glm::degrees(atan2(
			camFwd.x * input.moveDir.y + camRight.x * input.moveDir.x,
			camFwd.z * input.moveDir.y + camRight.z * input.moveDir.x));
		if (std::abs(input.moveDir.y) > 0.1f || std::abs(input.moveDir.x) > 0.1f)
			facingAngle = lerpAngle(facingAngle, wishAngle, TURN_SPEED * 0.3f * dt);

		// Swim direction: horizontal facing + vertical pitch
		float pitchRad = glm::radians(swimPitch);
		float fAngle = glm::radians(facingAngle);
		glm::vec3 swimDir(
			sinf(fAngle) * cosf(pitchRad),
			sinf(pitchRad),
			cosf(fAngle) * cosf(pitchRad));

		glm::vec3 targetVel = swimDir * SWIM_UNDERWATER_SPEED * input.moveStrength;
		glm::vec3 accel = targetVel - velocity;
		float accelLen = glm::length(accel);
		if (accelLen > 0.001f)
		{
			float maxAccel = AIR_ACCEL * 2.f * dt;
			if (accelLen > maxAccel)
				accel = accel / accelLen * maxAccel;
			velocity += accel;
		}
	}
	else
	{
		swimPitch *= (1.f - 3.f * dt);

		// Decelerate
		float speed = glm::length(velocity);
		if (speed > 0.1f)
			velocity *= std::max(0.f, 1.f - 3.f * dt);
		else
			velocity = {};
	}

	// Swim burst
	if (input.jump)
	{
		float pitchRad = glm::radians(swimPitch);
		float fAngle = glm::radians(facingAngle);
		glm::vec3 burstDir(
			sinf(fAngle) * cosf(pitchRad),
			sinf(pitchRad),
			cosf(fAngle) * cosf(pitchRad));
		velocity = burstDir * SWIM_BURST_SPEED;
	}

	// Reduced gravity
	velocity.y -= SWIM_GRAVITY * dt;
	if (velocity.y < -SWIM_TERMINAL_VELOCITY)
		velocity.y = -SWIM_TERMINAL_VELOCITY;

	// Air drain
	airTimer += dt;
	if (airTimer >= AIR_DRAIN_INTERVAL)
	{
		airTimer -= AIR_DRAIN_INTERVAL;
		health--;
		if (health <= 0)
		{
			health = 0;
			die();
			return;
		}
	}

	// Keep inside water volume bounds
	position.x = std::clamp(position.x, wv.minBounds.x + COLLISION_RADIUS, wv.maxBounds.x - COLLISION_RADIUS);
	position.z = std::clamp(position.z, wv.minBounds.z + COLLISION_RADIUS, wv.maxBounds.z - COLLISION_RADIUS);
	if (position.y < wv.minBounds.y)
	{
		position.y = wv.minBounds.y;
		velocity.y = std::max(velocity.y, 0.f);
	}
}

// ---- Main Update ----

void Mario::update(const GameInput &input, float dt, const glm::vec3 &cameraForward,
	const CollisionWorld *world, Phase5World *phase5World)
{
	groundPoundImpact = false;

	if (invincibilityTimer > 0.f)
		invincibilityTimer = std::max(0.f, invincibilityTimer - dt);

	if (onGround)
		airborneMaxY = position.y;

	if (isDead())
	{
		updateDeath(input, dt, cameraForward);
		position += velocity * dt;
		resolveCollision(world, phase5World, dt, position);
		return;
	}

	updateInputBuffers(input, dt);
	glm::vec3 previousPosition = position;

	bool controlledSpecialState = state == MarioState::LEDGE_HANG || state == MarioState::LEDGE_CLIMB ||
		state == MarioState::POLE_GRAB || state == MarioState::POLE_CLIMB ||
		state == MarioState::CARRY_IDLE || state == MarioState::CARRY_WALK ||
		state == MarioState::THROW || state == MarioState::DROP ||
		state == MarioState::SWIMMING_SURFACE || state == MarioState::SWIMMING_UNDERWATER;

	if (!controlledSpecialState && phase5World)
	{
		if (!onGround)
		{
			int pole = findNearestPole(*phase5World, position, 0.25f);
			if (pole >= 0)
			{
				activePole = pole;
				enterState(MarioState::POLE_GRAB);
				controlledSpecialState = true;
			}
		}

		if (!controlledSpecialState && onGround && input.attack && carriedObject < 0)
		{
			int objectIndex = findNearestCarriable(*phase5World, position, getFacingForward(), 1.4f);
			if (objectIndex >= 0)
			{
				carriedObject = objectIndex;
				setCarriedObject(*phase5World, objectIndex, true);
				actionTimer = 0.15f;
				enterState(MarioState::CARRY_IDLE);
				controlledSpecialState = true;
			}
		}
	}

	if (!controlledSpecialState && !onGround && wallContactTimer > 0.f)
	{
		if (jumpBufferTimer > 0.f)
		{
			jumpBufferTimer = 0.f;
			velocity = lastWallNormal * WALL_JUMP_HORIZONTAL + glm::vec3(0.f, WALL_JUMP_VERTICAL, 0.f);
			facingAngle = glm::degrees(atan2(lastWallNormal.x, lastWallNormal.z));
			enterState(MarioState::WALL_JUMP);
		}
		else if (velocity.y < 0.f && world && world->isLoaded())
		{
			glm::vec3 probe = position + lastWallNormal * 0.75f + glm::vec3(0.f, 1.2f, 0.f);
			CollisionHit ledgeGround;
			if (queryGround(*world, probe, COLLISION_RADIUS, 0.8f, 1.8f, ledgeGround))
			{
				ledgeTarget = {probe.x, ledgeGround.y, probe.z};
				enterState(MarioState::LEDGE_HANG);
				controlledSpecialState = true;
			}
			else
			{
				enterState(MarioState::WALL_SLIDE);
			}
		}
	}

	switch (state)
	{
	case MarioState::IDLE:               updateIdle(input, dt, cameraForward); break;
	case MarioState::WALKING:            updateWalking(input, dt, cameraForward); break;
	case MarioState::RUNNING:            updateRunning(input, dt, cameraForward); break;
	case MarioState::SKIDDING:           updateSkidding(input, dt, cameraForward); break;
	case MarioState::CROUCHING:          updateCrouching(input, dt, cameraForward); break;
	case MarioState::CRAWLING:           updateCrawling(input, dt, cameraForward); break;
	case MarioState::SINGLE_JUMP:
	case MarioState::DOUBLE_JUMP:
	case MarioState::TRIPLE_JUMP:
	case MarioState::BACKFLIP:
	case MarioState::SIDE_SOMERSAULT:
	case MarioState::FREEFALL:
	case MarioState::JUMP_KICK:
	case MarioState::DIVE:               updateAirborne(input, dt, cameraForward); break;
	case MarioState::LONG_JUMP:          updateLongJump(input, dt, cameraForward); break;
	case MarioState::GROUND_POUND_SPIN:  updateGroundPoundSpin(input, dt, cameraForward); break;
	case MarioState::GROUND_POUND_FALL:  updateGroundPoundFall(input, dt, cameraForward); break;
	case MarioState::WALL_SLIDE:         updateWallSlide(input, dt, cameraForward); break;
	case MarioState::WALL_JUMP:          updateAirborne(input, dt, cameraForward); break;
	case MarioState::LEDGE_HANG:         updateLedgeHang(input, dt, cameraForward); break;
	case MarioState::LEDGE_CLIMB:        updateLedgeClimb(input, dt, cameraForward); break;
	case MarioState::POLE_GRAB:
	case MarioState::POLE_CLIMB:         updatePole(input, dt, cameraForward, phase5World); break;
	case MarioState::CARRY_IDLE:
	case MarioState::CARRY_WALK:         updateCarry(input, dt, cameraForward, phase5World); break;
	case MarioState::THROW:
	case MarioState::DROP:               updateThrowDrop(input, dt, cameraForward, phase5World); break;
	case MarioState::GROUND_POUND_LAND:
	case MarioState::PUNCH_1:
	case MarioState::PUNCH_2:
	case MarioState::PUNCH_3_KICK:       updateTimedState(input, dt, cameraForward); break;
	case MarioState::BELLY_SLIDE:        updateBellySlide(input, dt, cameraForward); break;
	case MarioState::SLIDE_KICK:         updateSlideKick(input, dt, cameraForward); break;
	case MarioState::LANDING:            updateLanding(input, dt, cameraForward); break;
	case MarioState::KNOCKBACK:          updateKnockback(input, dt, cameraForward); break;
	case MarioState::DEATH:              break;
	case MarioState::SWIMMING_SURFACE:   updateSwimmingSurface(input, dt, cameraForward, phase5World); break;
	case MarioState::SWIMMING_UNDERWATER: updateSwimmingUnderwater(input, dt, cameraForward, phase5World); break;
	}

	position += velocity * dt;

	bool isSwimming = state == MarioState::SWIMMING_SURFACE || state == MarioState::SWIMMING_UNDERWATER;

	if (position.y < VOID_DEATH_Y && !isDead() && !isSwimming)
	{
		die();
		return;
	}

	if (!isSwimming)
		resolveCollision(world, phase5World, dt, previousPosition);

	// Water entry detection
	if (!isSwimming && !isDead() && phase5World)
	{
		int waterVol = findWaterVolume(*phase5World, position);
		if (waterVol >= 0)
		{
			currentWaterVolume = waterVol;
			enterWater(phase5World->waterVolumes[waterVol].surfaceY);
		}
	}
}
