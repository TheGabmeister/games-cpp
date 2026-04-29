#pragma once
#include <glm/glm.hpp>
#include "input.h"

enum class MarioState
{
	IDLE,
	WALKING,
	RUNNING,
	SKIDDING,
	JUMP_ASCEND,
	FREEFALL,
	LANDING,
};

const char *marioStateName(MarioState s);

struct GroundResult
{
	bool hit = false;
	float groundY = 0.f;
	glm::vec3 normal = {0.f, 1.f, 0.f};
};

struct Mario
{
	glm::vec3 position = {0.f, 0.f, 0.f};
	glm::vec3 velocity = {0.f, 0.f, 0.f};
	float facingAngle = 0.f;
	MarioState state = MarioState::IDLE;
	bool onGround = true;

	static constexpr float WALK_SPEED = 8.f;
	static constexpr float RUN_SPEED = 16.f;
	static constexpr float GROUND_ACCEL = 24.f;
	static constexpr float GROUND_DECEL = 32.f;
	static constexpr float AIR_ACCEL = 7.f;
	static constexpr float GRAVITY = 38.f;
	static constexpr float GRAVITY_HELD = 38.f * 0.6f;
	static constexpr float TERMINAL_VELOCITY = 75.f;
	static constexpr float JUMP_VELOCITY = 22.f;
	static constexpr float SKID_THRESHOLD = 10.f;
	static constexpr float TURN_SPEED = 720.f;
	static constexpr float HEIGHT = 1.5f;

	void update(const GameInput &input, float dt, const glm::vec3 &cameraForward);

private:
	void applyGroundMovement(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void applyAirMovement(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void applyGravity(const GameInput &input, float dt);
	GroundResult checkGround() const;
	glm::vec3 inputToWorldDir(const GameInput &input, const glm::vec3 &cameraForward) const;

	float skidTimer = 0.f;
	float landingTimer = 0.f;
};
