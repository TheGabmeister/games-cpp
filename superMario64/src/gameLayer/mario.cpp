#include "mario.h"
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
	case MarioState::JUMP_ASCEND: return "JUMP_ASCEND";
	case MarioState::FREEFALL: return "FREEFALL";
	case MarioState::LANDING: return "LANDING";
	}
	return "UNKNOWN";
}

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

void Mario::applyGravity(const GameInput &input, float dt)
{
	float g = GRAVITY;
	if (state == MarioState::JUMP_ASCEND && input.jumpHeld && velocity.y > 0.f)
		g = GRAVITY_HELD;

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

		// Check for skid: moving fast and input opposes velocity
		if (hSpeed > SKID_THRESHOLD)
		{
			float velAngle = glm::degrees(atan2(velocity.x, velocity.z));
			float diff = std::abs(angleDiff(wishAngle, velAngle));
			if (diff > 120.f)
			{
				state = MarioState::SKIDDING;
				skidTimer = 0.15f;
			}
		}

		if (state != MarioState::SKIDDING)
		{
			// Accelerate toward wish direction
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
		// Decelerate
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

void Mario::update(const GameInput &input, float dt, const glm::vec3 &cameraForward)
{

	// Skid timer
	if (state == MarioState::SKIDDING)
	{
		skidTimer -= dt;
		float decel = GROUND_DECEL * 1.5f * dt;
		glm::vec2 hVel(velocity.x, velocity.z);
		float hSpeed = glm::length(hVel);
		if (hSpeed > decel)
			hVel *= (hSpeed - decel) / hSpeed;
		else
			hVel = {0, 0};
		velocity.x = hVel.x;
		velocity.z = hVel.y;

		if (skidTimer <= 0.f)
			state = MarioState::IDLE;
	}

	// Landing timer
	if (state == MarioState::LANDING)
	{
		landingTimer -= dt;
		if (landingTimer <= 0.f)
			state = MarioState::IDLE;
	}

	if (onGround)
	{
		if (state != MarioState::SKIDDING && state != MarioState::LANDING)
			applyGroundMovement(input, dt, cameraForward);

		if (input.jump)
		{
			velocity.y = JUMP_VELOCITY;
			onGround = false;
			state = MarioState::JUMP_ASCEND;
		}
	}
	else
	{
		applyAirMovement(input, dt, cameraForward);
		applyGravity(input, dt);

		if (state == MarioState::JUMP_ASCEND && velocity.y <= 0.f)
			state = MarioState::FREEFALL;
	}

	position += velocity * dt;

	// Ground check
	GroundResult ground = checkGround();
	if (ground.hit && velocity.y <= 0.f)
	{
		position.y = ground.groundY;
		velocity.y = 0.f;
		if (!onGround)
		{
			state = MarioState::LANDING;
			landingTimer = 0.08f;
		}
		onGround = true;
	}
	else if (!ground.hit)
	{
		onGround = false;
		if (state != MarioState::JUMP_ASCEND && state != MarioState::FREEFALL)
			state = MarioState::FREEFALL;
	}
}
