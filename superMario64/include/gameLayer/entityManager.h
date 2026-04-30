#pragma once
#include <vector>
#include <memory>
#include "entity.h"

struct EntityManager
{
	std::vector<std::unique_ptr<Entity>> entities;

	void addEntity(std::unique_ptr<Entity> entity);
	void update(float dt, const glm::vec3 &marioPos,
		float marioFacingAngle, const CollisionWorld *world);
	void spawnDespawn(const glm::vec3 &marioPos);

	int checkEnemyContact(const glm::vec3 &marioPos, float marioRadius) const;
	int checkAttackHit(const glm::vec3 &attackPos, float attackRadius) const;
	int findGrabbable(const glm::vec3 &marioPos, float marioFacing, float grabRadius) const;

	void defeatEnemy(int index);
	void clear();
};
