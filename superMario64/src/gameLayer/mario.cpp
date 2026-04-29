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
	case MarioState::LANDING: return "LANDING";
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
	case MarioState::LANDING: return "landing";
	}
	return "idle";
}

void Mario::setAnimClips(const SkinnedModel &model)
{
	for (int i = 0; i < 24; i++)
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

GroundResult Mario::checkGround() const
{
	GroundResult r;
	if (position.y <= 0.f)
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
	                      state == MarioState::SIDE_SOMERSAULT);
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
				float maxAccel = GROUND_ACCEL * dt;
				if (accelLen > maxAccel)
					accel = accel / accelLen * maxAccel;
				hVel += accel;
			}

			facingAngle = lerpAngle(facingAngle, wishAngle, TURN_SPEED * dt);

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
		float decel = GROUND_DECEL * dt;
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
	jumpChainCount = 1;
	enterState(MarioState::SINGLE_JUMP);
}

// ---- Ground Collision ----

void Mario::resolveGroundCollision(float dt)
{
	GroundResult ground = checkGround();

	if (ground.hit && velocity.y <= 0.f)
	{
		position.y = ground.groundY;
		velocity.y = 0.f;

		if (!onGround)
		{
			// Just landed — jumpChainCount was already set in tryGroundJump

			jumpChainTimer = 0.f;

			// Special landing states
			if (state == MarioState::GROUND_POUND_FALL)
			{
				onGround = true;
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
				jumpBufferTimer = 0.f;
				// Pass last known input — we need cameraForward but don't have it here.
				// The buffered jump will be handled next frame from the landing state.
				// Actually, for responsive feel we should do it here. We store cameraForward
				// isn't available in this function, so we enter LANDING and tryGroundJump
				// will fire on the next frame from updateLanding.
			}

			onGround = true;
			enterState(MarioState::LANDING);
			return;
		}

		onGround = true;
	}
	else if (!ground.hit)
	{
		if (onGround)
		{
			// Just left ground — start coyote timer (already set in updateInputBuffers)
			onGround = false;
		}

		if (state != MarioState::SINGLE_JUMP && state != MarioState::DOUBLE_JUMP &&
		    state != MarioState::TRIPLE_JUMP && state != MarioState::LONG_JUMP &&
		    state != MarioState::BACKFLIP && state != MarioState::SIDE_SOMERSAULT &&
		    state != MarioState::FREEFALL && state != MarioState::GROUND_POUND_SPIN &&
		    state != MarioState::GROUND_POUND_FALL && state != MarioState::JUMP_KICK &&
		    state != MarioState::DIVE && state != MarioState::SLIDE_KICK)
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
	     state == MarioState::SIDE_SOMERSAULT) && velocity.y <= 0.f)
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

// ---- Main Update ----

void Mario::update(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{
	updateInputBuffers(input, dt);

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
	case MarioState::GROUND_POUND_LAND:
	case MarioState::PUNCH_1:
	case MarioState::PUNCH_2:
	case MarioState::PUNCH_3_KICK:       updateTimedState(input, dt, cameraForward); break;
	case MarioState::BELLY_SLIDE:        updateBellySlide(input, dt, cameraForward); break;
	case MarioState::SLIDE_KICK:         updateSlideKick(input, dt, cameraForward); break;
	case MarioState::LANDING:            updateLanding(input, dt, cameraForward); break;
	}

	position += velocity * dt;
	resolveGroundCollision(dt);
}
