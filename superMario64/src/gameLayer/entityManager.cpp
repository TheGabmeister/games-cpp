#include "entityManager.h"
#include <cmath>

void EntityManager::addEntity(std::unique_ptr<Entity> entity)
{
	entities.push_back(std::move(entity));
}

void EntityManager::spawnDespawn(const glm::vec3 &marioPos)
{
	for (auto &entity : entities)
	{
		float dist = entity->distanceToXZ(marioPos);

		if (!entity->active)
		{
			if (dist < entity->spawnRadius && !entity->defeatedThisCycle)
			{
				entity->active = true;
				entity->onSpawn();
			}
			if (dist > entity->despawnRadius)
				entity->defeatedThisCycle = false;
		}
		else
		{
			if (dist > entity->despawnRadius)
			{
				entity->active = false;
				entity->onDespawn();
			}
		}
	}
}

void EntityManager::update(float dt, const glm::vec3 &marioPos,
	float marioFacingAngle, const CollisionWorld *world)
{
	for (auto &entity : entities)
	{
		if (!entity->active || !entity->alive) continue;
		entity->update(dt, marioPos, marioFacingAngle, world);
	}
}

int EntityManager::checkEnemyContact(const glm::vec3 &marioPos, float marioRadius) const
{
	for (int i = 0; i < (int)entities.size(); i++)
	{
		const Entity *e = entities[i].get();
		if (!e->active || !e->alive) continue;
		if (e->aiState == EnemyAIState::Defeated || e->aiState == EnemyAIState::Grabbed) continue;
		// Boo is intangible when transparent
		if (e->type == EntityType::Boo && e->transparency > 0.3f) continue;

		float dist = e->distanceToXZ(marioPos);
		if (dist < e->collisionRadius + marioRadius)
		{
			float dy = fabsf(marioPos.y - e->position.y);
			if (dy < e->height)
				return i;
		}
	}
	return -1;
}

int EntityManager::checkAttackHit(const glm::vec3 &attackPos, float attackRadius) const
{
	for (int i = 0; i < (int)entities.size(); i++)
	{
		const Entity *e = entities[i].get();
		if (!e->active || !e->alive) continue;
		if (e->aiState == EnemyAIState::Defeated || e->aiState == EnemyAIState::Grabbed ||
			e->aiState == EnemyAIState::Exploding) continue;
		// Boo is only damageable when not transparent (Mario is facing it)
		if (e->type == EntityType::Boo)
		{
			const BooEntity *boo = static_cast<const BooEntity *>(e);
			if (!boo->marioFacing) continue;
		}

		float dist = e->distanceToXZ(attackPos);
		if (dist < e->collisionRadius + attackRadius)
		{
			float dy = fabsf(attackPos.y - e->position.y);
			if (dy < e->height + 0.5f)
				return i;
		}
	}
	return -1;
}

int EntityManager::findGrabbable(const glm::vec3 &marioPos, float marioFacing, float grabRadius) const
{
	float bestDist = grabRadius;
	int bestIndex = -1;

	for (int i = 0; i < (int)entities.size(); i++)
	{
		const Entity *e = entities[i].get();
		if (!e->active || !e->alive) continue;
		if (e->type != EntityType::BobOmb) continue;

		const BobOmbEntity *bob = static_cast<const BobOmbEntity *>(e);
		if (!bob->isGrabbableFrom(marioPos, marioFacing)) continue;

		float dist = e->distanceToXZ(marioPos);
		if (dist < bestDist)
		{
			bestDist = dist;
			bestIndex = i;
		}
	}
	return bestIndex;
}

void EntityManager::defeatEnemy(int index)
{
	if (index < 0 || index >= (int)entities.size()) return;
	Entity *e = entities[index].get();
	e->defeatedThisCycle = true;
	e->onDefeated();
}

void EntityManager::clear()
{
	entities.clear();
}
