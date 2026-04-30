#pragma once
#include <glm/glm.hpp>
#include "animation.h"

struct CollisionWorld;

enum class EntityType
{
	Goomba,
	BobOmb,
	KoopaTroopa,
	Boo,
	KingBobOmb,
	ChainChomp,
	COUNT,
};

enum class EnemyAIState
{
	Idle,
	Patrol,
	Chase,
	Flee,
	Attack,
	Stunned,
	Defeated,
	Grabbed,
	Thrown,
	Exploding,
};

struct Entity
{
	EntityType type = EntityType::Goomba;
	glm::vec3 position = {};
	glm::vec3 velocity = {};
	glm::vec3 spawnPosition = {};
	float facingAngle = 0.f;
	float collisionRadius = 0.5f;
	float height = 1.0f;
	bool alive = true;
	bool active = false;
	bool defeatedThisCycle = false;

	float spawnRadius = 50.f;
	float despawnRadius = 60.f;

	EnemyAIState aiState = EnemyAIState::Patrol;
	float aiTimer = 0.f;
	float detectionRadius = 10.f;
	glm::vec3 patrolPointA = {};
	glm::vec3 patrolPointB = {};
	bool patrolForward = true;

	bool onGround = false;
	float groundY = 0.f;

	int health = 1;
	int coinDropValue = 1;
	bool dropsCoin = true;

	AnimState animState;

	virtual ~Entity() = default;
	virtual void update(float dt, const glm::vec3 &marioPos,
		float marioFacingAngle, const CollisionWorld *world) = 0;
	virtual void onDefeated() {}
	virtual void onSpawn();
	virtual void onDespawn() {}

	void applyGravity(float dt);
	void resolveGround(const CollisionWorld *world);
	bool checkLedgeAhead(const CollisionWorld *world, float lookAhead) const;
	void faceToward(const glm::vec3 &target, float turnSpeed, float dt);
	void faceAwayFrom(const glm::vec3 &target, float turnSpeed, float dt);
	void moveForward(float speed, float dt);
	float distanceToXZ(const glm::vec3 &target) const;
	glm::vec3 getFacingForward() const;

	float transparency = 0.f;
};

struct GoombaEntity : Entity
{
	static constexpr float WALK_SPEED = 2.5f;
	static constexpr float CHARGE_SPEED = 6.f;
	static constexpr float SQUISH_DURATION = 0.5f;
	float squishTimer = 0.f;

	GoombaEntity();
	void update(float dt, const glm::vec3 &marioPos,
		float marioFacingAngle, const CollisionWorld *world) override;
	void onDefeated() override;
};

struct BobOmbEntity : Entity
{
	static constexpr float WALK_SPEED = 2.0f;
	static constexpr float CHASE_SPEED = 5.f;
	static constexpr float FUSE_DURATION = 3.f;
	static constexpr float EXPLOSION_RADIUS = 4.f;
	static constexpr float EXPLOSION_DURATION = 0.5f;
	float fuseTimer = 0.f;
	float explosionTimer = 0.f;
	bool lit = false;

	BobOmbEntity();
	void update(float dt, const glm::vec3 &marioPos,
		float marioFacingAngle, const CollisionWorld *world) override;
	void onDefeated() override;
	bool isGrabbableFrom(const glm::vec3 &marioPos, float marioFacing) const;
};

struct KoopaTroopaEntity : Entity
{
	static constexpr float WALK_SPEED = 2.0f;
	static constexpr float FLEE_SPEED = 5.f;

	KoopaTroopaEntity();
	void update(float dt, const glm::vec3 &marioPos,
		float marioFacingAngle, const CollisionWorld *world) override;
	void onDefeated() override;
};

struct BooEntity : Entity
{
	static constexpr float APPROACH_SPEED = 3.f;
	static constexpr float TRANSPARENCY_THRESHOLD = 90.f;
	bool marioFacing = false;

	BooEntity();
	void update(float dt, const glm::vec3 &marioPos,
		float marioFacingAngle, const CollisionWorld *world) override;
	void onDefeated() override;
};

const char *entityTypeName(EntityType type);
const char *enemyAIStateName(EnemyAIState state);
