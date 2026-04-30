#define GLM_ENABLE_EXPERIMENTAL
#include "entity.h"
#include "world.h"
#include <glm/gtx/transform.hpp>
#include <cmath>
#include <algorithm>

// ---- Entity base ----

const char *entityTypeName(EntityType type)
{
	switch (type)
	{
	case EntityType::Goomba: return "Goomba";
	case EntityType::BobOmb: return "Bob-omb";
	case EntityType::KoopaTroopa: return "Koopa Troopa";
	case EntityType::Boo: return "Boo";
	case EntityType::KingBobOmb: return "King Bob-omb";
	case EntityType::ChainChomp: return "Chain Chomp";
	default: return "Unknown";
	}
}

const char *enemyAIStateName(EnemyAIState state)
{
	switch (state)
	{
	case EnemyAIState::Idle: return "Idle";
	case EnemyAIState::Patrol: return "Patrol";
	case EnemyAIState::Chase: return "Chase";
	case EnemyAIState::Flee: return "Flee";
	case EnemyAIState::Attack: return "Attack";
	case EnemyAIState::Stunned: return "Stunned";
	case EnemyAIState::Defeated: return "Defeated";
	case EnemyAIState::Grabbed: return "Grabbed";
	case EnemyAIState::Thrown: return "Thrown";
	case EnemyAIState::Exploding: return "Exploding";
	default: return "Unknown";
	}
}

void Entity::onSpawn()
{
	position = spawnPosition;
	velocity = {};
	health = 1;
	alive = true;
	aiState = EnemyAIState::Patrol;
	aiTimer = 0.f;
	onGround = false;
	transparency = 0.f;
	patrolForward = true;
}

void Entity::applyGravity(float dt)
{
	if (!onGround)
	{
		velocity.y -= 38.f * dt;
		if (velocity.y < -75.f) velocity.y = -75.f;
	}
	position += glm::vec3(0.f, velocity.y * dt, 0.f);
}

void Entity::resolveGround(const CollisionWorld *world)
{
	if (!world || !world->isLoaded()) return;
	CollisionHit hit;
	if (queryGround(*world, position, collisionRadius, 0.5f, 2.f, hit))
	{
		if (position.y <= hit.y + 0.05f)
		{
			position.y = hit.y;
			velocity.y = 0.f;
			onGround = true;
			groundY = hit.y;
		}
		else
		{
			onGround = false;
		}
	}
	else
	{
		onGround = false;
	}
}

bool Entity::checkLedgeAhead(const CollisionWorld *world, float lookAhead) const
{
	if (!world || !world->isLoaded()) return false;
	glm::vec3 ahead = position + getFacingForward() * lookAhead;
	CollisionHit hit;
	bool hasGround = queryGround(*world, ahead, collisionRadius, 0.5f, 3.f, hit);
	return !hasGround || (hit.y < position.y - 1.5f);
}

void Entity::faceToward(const glm::vec3 &target, float turnSpeed, float dt)
{
	glm::vec3 diff = target - position;
	if (glm::length(glm::vec2(diff.x, diff.z)) < 0.001f) return;
	float targetAngle = glm::degrees(atan2f(diff.x, diff.z));

	float delta = targetAngle - facingAngle;
	while (delta > 180.f) delta -= 360.f;
	while (delta < -180.f) delta += 360.f;

	float maxTurn = turnSpeed * dt;
	if (fabsf(delta) <= maxTurn)
		facingAngle = targetAngle;
	else
		facingAngle += (delta > 0.f ? maxTurn : -maxTurn);

	while (facingAngle > 360.f) facingAngle -= 360.f;
	while (facingAngle < 0.f) facingAngle += 360.f;
}

void Entity::faceAwayFrom(const glm::vec3 &target, float turnSpeed, float dt)
{
	glm::vec3 diff = position - target;
	if (glm::length(glm::vec2(diff.x, diff.z)) < 0.001f) return;
	float targetAngle = glm::degrees(atan2f(diff.x, diff.z));

	float delta = targetAngle - facingAngle;
	while (delta > 180.f) delta -= 360.f;
	while (delta < -180.f) delta += 360.f;

	float maxTurn = turnSpeed * dt;
	if (fabsf(delta) <= maxTurn)
		facingAngle = targetAngle;
	else
		facingAngle += (delta > 0.f ? maxTurn : -maxTurn);

	while (facingAngle > 360.f) facingAngle -= 360.f;
	while (facingAngle < 0.f) facingAngle += 360.f;
}

void Entity::moveForward(float speed, float dt)
{
	glm::vec3 forward = getFacingForward();
	position.x += forward.x * speed * dt;
	position.z += forward.z * speed * dt;
}

float Entity::distanceToXZ(const glm::vec3 &target) const
{
	float dx = target.x - position.x;
	float dz = target.z - position.z;
	return sqrtf(dx * dx + dz * dz);
}

glm::vec3 Entity::getFacingForward() const
{
	float angle = glm::radians(facingAngle);
	return glm::normalize(glm::vec3(sinf(angle), 0.f, cosf(angle)));
}

// ---- Patrol helper ----

static void updatePatrol(Entity &e, float speed, float dt, const CollisionWorld *world)
{
	glm::vec3 &target = e.patrolForward ? e.patrolPointB : e.patrolPointA;
	float dist = e.distanceToXZ(target);
	if (dist < 0.5f)
	{
		e.patrolForward = !e.patrolForward;
		target = e.patrolForward ? e.patrolPointB : e.patrolPointA;
	}

	if (e.checkLedgeAhead(world, 1.0f))
	{
		e.patrolForward = !e.patrolForward;
		e.facingAngle += 180.f;
		if (e.facingAngle > 360.f) e.facingAngle -= 360.f;
	}
	else
	{
		e.faceToward(target, 360.f, dt);
		e.moveForward(speed, dt);
	}
}

// ---- Goomba ----

GoombaEntity::GoombaEntity()
{
	type = EntityType::Goomba;
	collisionRadius = 0.4f;
	height = 0.8f;
	coinDropValue = 1;
	detectionRadius = 10.f;
}

void GoombaEntity::update(float dt, const glm::vec3 &marioPos,
	float marioFacingAngle, const CollisionWorld *world)
{
	if (!alive) return;

	float dist = distanceToXZ(marioPos);

	switch (aiState)
	{
	case EnemyAIState::Patrol:
		updatePatrol(*this, WALK_SPEED, dt, world);
		if (dist < detectionRadius)
		{
			aiState = EnemyAIState::Chase;
			aiTimer = 0.f;
		}
		break;

	case EnemyAIState::Chase:
		if (checkLedgeAhead(world, 1.0f))
		{
			// Stop at edge, don't follow off
		}
		else
		{
			faceToward(marioPos, 540.f, dt);
			moveForward(CHARGE_SPEED, dt);
		}
		if (dist > detectionRadius * 1.5f)
		{
			aiState = EnemyAIState::Patrol;
		}
		break;

	case EnemyAIState::Defeated:
		squishTimer -= dt;
		if (squishTimer <= 0.f)
			alive = false;
		return;

	default:
		break;
	}

	applyGravity(dt);
	resolveGround(world);
}

void GoombaEntity::onDefeated()
{
	aiState = EnemyAIState::Defeated;
	squishTimer = SQUISH_DURATION;
	velocity = {};
}

// ---- Bob-omb ----

BobOmbEntity::BobOmbEntity()
{
	type = EntityType::BobOmb;
	collisionRadius = 0.4f;
	height = 0.8f;
	coinDropValue = 1;
	detectionRadius = 12.f;
}

void BobOmbEntity::update(float dt, const glm::vec3 &marioPos,
	float marioFacingAngle, const CollisionWorld *world)
{
	if (!alive) return;

	float dist = distanceToXZ(marioPos);

	switch (aiState)
	{
	case EnemyAIState::Patrol:
		updatePatrol(*this, WALK_SPEED, dt, world);
		if (dist < detectionRadius)
		{
			aiState = EnemyAIState::Chase;
			lit = true;
			fuseTimer = FUSE_DURATION;
		}
		break;

	case EnemyAIState::Chase:
		fuseTimer -= dt;
		if (fuseTimer <= 0.f)
		{
			aiState = EnemyAIState::Exploding;
			explosionTimer = EXPLOSION_DURATION;
			velocity = {};
			break;
		}
		if (checkLedgeAhead(world, 1.0f))
		{
			// Stop at edge
		}
		else
		{
			faceToward(marioPos, 540.f, dt);
			moveForward(CHASE_SPEED, dt);
		}
		if (dist > detectionRadius * 2.f)
		{
			aiState = EnemyAIState::Patrol;
			lit = false;
		}
		break;

	case EnemyAIState::Grabbed:
		// Position is controlled externally by Mario's carry logic
		return;

	case EnemyAIState::Thrown:
		applyGravity(dt);
		position.x += velocity.x * dt;
		position.z += velocity.z * dt;
		resolveGround(world);
		if (onGround)
		{
			aiState = EnemyAIState::Exploding;
			explosionTimer = EXPLOSION_DURATION;
			velocity = {};
		}
		return;

	case EnemyAIState::Exploding:
		explosionTimer -= dt;
		if (explosionTimer <= 0.f)
			alive = false;
		return;

	case EnemyAIState::Defeated:
		alive = false;
		return;

	default:
		break;
	}

	applyGravity(dt);
	resolveGround(world);
}

void BobOmbEntity::onDefeated()
{
	aiState = EnemyAIState::Defeated;
	velocity = {};
}

bool BobOmbEntity::isGrabbableFrom(const glm::vec3 &marioPos, float marioFacing) const
{
	if (aiState == EnemyAIState::Grabbed || aiState == EnemyAIState::Thrown ||
		aiState == EnemyAIState::Exploding || aiState == EnemyAIState::Defeated)
		return false;

	glm::vec3 forward = getFacingForward();
	glm::vec3 toMario = glm::normalize(glm::vec3(marioPos.x - position.x, 0.f, marioPos.z - position.z));
	float dot = glm::dot(forward, toMario);
	// Mario must be behind the Bob-omb (dot < 0 means Mario is behind)
	return dot < -0.3f;
}

// ---- Koopa Troopa ----

KoopaTroopaEntity::KoopaTroopaEntity()
{
	type = EntityType::KoopaTroopa;
	collisionRadius = 0.5f;
	height = 1.0f;
	coinDropValue = 5;
	detectionRadius = 8.f;
}

void KoopaTroopaEntity::update(float dt, const glm::vec3 &marioPos,
	float marioFacingAngle, const CollisionWorld *world)
{
	if (!alive) return;

	float dist = distanceToXZ(marioPos);

	switch (aiState)
	{
	case EnemyAIState::Patrol:
		updatePatrol(*this, WALK_SPEED, dt, world);
		if (dist < detectionRadius)
		{
			aiState = EnemyAIState::Flee;
		}
		break;

	case EnemyAIState::Flee:
		if (checkLedgeAhead(world, 1.0f))
		{
			// Stop at edge
		}
		else
		{
			faceAwayFrom(marioPos, 540.f, dt);
			moveForward(FLEE_SPEED, dt);
		}
		if (dist > detectionRadius * 2.f)
		{
			aiState = EnemyAIState::Patrol;
		}
		break;

	case EnemyAIState::Defeated:
		aiTimer -= dt;
		if (aiTimer <= 0.f)
			alive = false;
		return;

	default:
		break;
	}

	applyGravity(dt);
	resolveGround(world);
}

void KoopaTroopaEntity::onDefeated()
{
	aiState = EnemyAIState::Defeated;
	aiTimer = 0.5f;
	velocity = {};
}

// ---- Boo ----

BooEntity::BooEntity()
{
	type = EntityType::Boo;
	collisionRadius = 0.6f;
	height = 1.2f;
	coinDropValue = 1;
	detectionRadius = 15.f;
}

void BooEntity::update(float dt, const glm::vec3 &marioPos,
	float marioFacingAngle, const CollisionWorld *world)
{
	if (!alive) return;

	float dist = distanceToXZ(marioPos);

	// Check if Mario is facing the Boo
	float marioRad = glm::radians(marioFacingAngle);
	glm::vec3 marioForward(sinf(marioRad), 0.f, cosf(marioRad));
	glm::vec3 toBoo = glm::vec3(position.x - marioPos.x, 0.f, position.z - marioPos.z);
	float len = glm::length(toBoo);
	if (len > 0.001f) toBoo /= len;
	float dot = glm::dot(marioForward, toBoo);
	marioFacing = (dot > cosf(glm::radians(TRANSPARENCY_THRESHOLD)));

	switch (aiState)
	{
	case EnemyAIState::Idle:
		// Become transparent when Mario faces us
		if (marioFacing)
		{
			transparency = std::min(transparency + 2.f * dt, 0.8f);
		}
		else
		{
			transparency = std::max(transparency - 2.f * dt, 0.f);
			if (transparency <= 0.f && dist < detectionRadius)
			{
				aiState = EnemyAIState::Chase;
			}
		}
		break;

	case EnemyAIState::Chase:
		if (marioFacing)
		{
			aiState = EnemyAIState::Idle;
			velocity = {};
			break;
		}
		transparency = std::max(transparency - 2.f * dt, 0.f);
		faceToward(marioPos, 360.f, dt);
		moveForward(APPROACH_SPEED, dt);

		if (dist > detectionRadius * 2.f)
		{
			aiState = EnemyAIState::Idle;
		}
		break;

	case EnemyAIState::Defeated:
		aiTimer -= dt;
		transparency = std::min(transparency + 3.f * dt, 1.f);
		if (aiTimer <= 0.f)
			alive = false;
		return;

	default:
		break;
	}

	// Boo floats — gentle vertical bobbing instead of gravity
	float targetY = spawnPosition.y + sinf(aiTimer) * 0.3f;
	aiTimer += dt * 2.f;
	position.y += (targetY - position.y) * 2.f * dt;
}

void BooEntity::onDefeated()
{
	aiState = EnemyAIState::Defeated;
	aiTimer = 0.8f;
	velocity = {};
}
