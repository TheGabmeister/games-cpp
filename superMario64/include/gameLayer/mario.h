#pragma once
#include <glm/glm.hpp>
#include "input.h"
#include "animation.h"
#include "world.h"

struct SkinnedModel;

enum class MarioState
{
	IDLE,
	WALKING,
	RUNNING,
	SKIDDING,
	SINGLE_JUMP,
	DOUBLE_JUMP,
	TRIPLE_JUMP,
	LONG_JUMP,
	BACKFLIP,
	SIDE_SOMERSAULT,
	FREEFALL,
	GROUND_POUND_SPIN,
	GROUND_POUND_FALL,
	GROUND_POUND_LAND,
	CROUCHING,
	CRAWLING,
	PUNCH_1,
	PUNCH_2,
	PUNCH_3_KICK,
	JUMP_KICK,
	DIVE,
	BELLY_SLIDE,
	SLIDE_KICK,
	WALL_SLIDE,
	WALL_JUMP,
	LEDGE_HANG,
	LEDGE_CLIMB,
	POLE_GRAB,
	POLE_CLIMB,
	CARRY_IDLE,
	CARRY_WALK,
	THROW,
	DROP,
	LANDING,
	KNOCKBACK,
	DEATH,
	COUNT,
};

const char *marioStateName(MarioState s);

struct GroundResult
{
	bool hit = false;
	float groundY = 0.f;
	glm::vec3 normal = {0.f, 1.f, 0.f};
	SurfaceType surface = SurfaceType::Normal;
	SlopeClass slopeClass = SlopeClass::Walkable;
};

struct Mario
{
	glm::vec3 position = {0.f, 0.f, 0.f};
	glm::vec3 velocity = {0.f, 0.f, 0.f};
	float facingAngle = 0.f;
	MarioState state = MarioState::IDLE;
	bool onGround = true;
	glm::vec3 groundNormal = {0.f, 1.f, 0.f};
	SurfaceType groundSurface = SurfaceType::Normal;
	SlopeClass groundSlope = SlopeClass::Walkable;
	glm::vec3 lastWallNormal = {0.f, 0.f, 0.f};
	float wallContactTimer = 0.f;
	glm::vec3 ledgeTarget = {};
	int activePole = -1;
	int carriedObject = -1;
	int currentPlatform = -1;
	bool groundPoundImpact = false;

	// Jump chain tracking
	int jumpChainCount = 0;
	float jumpChainTimer = 0.f;

	// Input buffering
	float jumpBufferTimer = 0.f;
	float coyoteTimer = 0.f;

	// Combat
	int punchComboStep = 0;
	float comboTimer = 0.f;

	// General-purpose state timer
	float stateTimer = 0.f;

	// Health & damage
	int health = 8;
	int maxHealth = 8;
	int lives = 4;
	int coins = 0;
	int stars = 0;
	float invincibilityTimer = 0.f;
	float airborneMaxY = 0.f;
	float deathTimer = 0.f;
	glm::vec3 spawnPosition = {0.f, 0.f, 0.f};
	float healAccumulator = 0.f;

	// Movement constants
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
	static constexpr float COLLISION_RADIUS = 0.35f;
	static constexpr float STEP_HEIGHT = 0.3f;

	// Jump variant constants
	static constexpr float DOUBLE_JUMP_VELOCITY = 28.f;
	static constexpr float TRIPLE_JUMP_VELOCITY = 33.f;
	static constexpr float TRIPLE_JUMP_SPEED_THRESHOLD = 12.f;
	static constexpr float LONG_JUMP_VERTICAL = 15.f;
	static constexpr float LONG_JUMP_HORIZONTAL = 32.f;
	static constexpr float BACKFLIP_VELOCITY = 35.f;
	static constexpr float SIDE_SOMERSAULT_VELOCITY = 30.f;
	static constexpr float GROUND_POUND_GRAVITY_MULT = 2.f;

	// Timing windows (in seconds)
	static constexpr float JUMP_BUFFER_WINDOW = 8.f / 60.f;
	static constexpr float COYOTE_TIME = 5.f / 60.f;
	static constexpr float JUMP_CHAIN_WINDOW = 20.f / 60.f;
	static constexpr float COMBO_WINDOW = 10.f / 60.f;

	// Action speeds
	static constexpr float CRAWL_SPEED = 3.f;
	static constexpr float DIVE_HORIZONTAL_SPEED = 20.f;
	static constexpr float SLIDE_KICK_SPEED = 18.f;
	static constexpr float WALL_JUMP_HORIZONTAL = 18.f;
	static constexpr float WALL_JUMP_VERTICAL = 24.f;
	static constexpr float POLE_CLIMB_SPEED = 4.f;

	// Health & damage constants
	static constexpr float INVINCIBILITY_DURATION = 2.f;
	static constexpr float FALL_DAMAGE_THRESHOLD = 11.f;
	static constexpr int FALL_DAMAGE_SEGMENTS = 3;
	static constexpr float VOID_DEATH_Y = -20.f;
	static constexpr float DEATH_DURATION = 2.f;
	static constexpr float KNOCKBACK_SPEED = 12.f;

	AnimState animState;

	void update(const GameInput &input, float dt, const glm::vec3 &cameraForward,
		const CollisionWorld *world = nullptr, Phase5World *phase5World = nullptr);
	void setAnimClips(const SkinnedModel &model);

	void takeDamage(int segments, const glm::vec3 &sourcePos);
	void heal(int segments);
	void collectCoin(int value);
	void die();
	bool isInvincible() const;
	bool isDead() const;
	void respawn();

private:
	void updateInputBuffers(const GameInput &input, float dt);
	void updateIdle(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateWalking(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateRunning(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateSkidding(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateCrouching(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateCrawling(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateAirborne(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateLongJump(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateGroundPoundSpin(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateGroundPoundFall(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateTimedState(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateBellySlide(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateSlideKick(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateWallSlide(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateLedgeHang(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateLedgeClimb(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updatePole(const GameInput &input, float dt, const glm::vec3 &cameraForward, Phase5World *phase5World);
	void updateCarry(const GameInput &input, float dt, const glm::vec3 &cameraForward, Phase5World *phase5World);
	void updateThrowDrop(const GameInput &input, float dt, const glm::vec3 &cameraForward, Phase5World *phase5World);
	void updateLanding(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateKnockback(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void updateDeath(const GameInput &input, float dt, const glm::vec3 &cameraForward);

	void resolveCollision(const CollisionWorld *world, Phase5World *phase5World, float dt, const glm::vec3 &previousPosition);
	void tryGroundJump(const GameInput &input, const glm::vec3 &cameraForward);
	void enterState(MarioState newState);
	float getHorizontalSpeed() const;
	glm::vec3 getFacingForward() const;

	void applyGroundMovement(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void applyAirMovement(const GameInput &input, float dt, const glm::vec3 &cameraForward);
	void applyGravity(const GameInput &input, float dt);
	GroundResult checkGround(const CollisionWorld *world, Phase5World *phase5World,
		float previousY, float maxAbove, float maxBelow, int *platformIndex = nullptr) const;
	glm::vec3 inputToWorldDir(const GameInput &input, const glm::vec3 &cameraForward) const;

	float skidTimer = 0.f;
	float landingTimer = 0.f;
	float ledgeTimer = 0.f;
	float actionTimer = 0.f;

	int clipForState[(int)MarioState::COUNT] = {};
	bool hasAnimClips = false;
};
