#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "world.h"
#include "entity.h"
#include "entityManager.h"
#include "renderer.h"

struct CourseStarDef
{
	int starIndex = 0;
	std::string name;
};

struct CourseEnemyDef
{
	EntityType type = EntityType::Goomba;
	glm::vec3 position = {};
	glm::vec3 patrolA = {};
	glm::vec3 patrolB = {};
	float facingAngle = 0.f;
	float spawnRadius = 50.f;
	float detectionRadius = 10.f;
};

struct CoursePlatformDef
{
	Phase5ObjectType type = Phase5ObjectType::MovingPlatform;
	glm::vec3 position = {};
	glm::vec3 endPosition = {};
	glm::vec3 halfExtents = {1.f, 0.2f, 1.f};
};

struct CourseCollectibleDef
{
	CollectibleType type = CollectibleType::YellowCoin;
	glm::vec3 position = {};
};

struct CoursePoleDef
{
	glm::vec3 base = {};
	float height = 4.f;
};

struct CourseWaterDef
{
	glm::vec3 minBounds = {};
	glm::vec3 maxBounds = {};
	float surfaceY = 0.f;
};

struct CourseDef
{
	std::string name;
	std::string visualMeshPath;
	std::string collisionMeshPath;
	glm::vec3 marioSpawn = {};
	float marioSpawnAngle = 0.f;
	std::vector<CourseStarDef> stars;
	std::vector<CourseEnemyDef> enemies;
	std::vector<CoursePlatformDef> platforms;
	std::vector<CourseCollectibleDef> collectibles;
	std::vector<CoursePoleDef> poles;
	std::vector<CourseWaterDef> water;
};

struct LoadedCourse
{
	CourseDef def;
	Mesh visualMesh;
	CollisionWorld collisionWorld;
	LineMesh collisionWireMesh;
	Phase5World phase5World;
	EntityManager entityManager;
	bool loaded = false;
	int redCoinsCollected = 0;
	bool hundredCoinStarSpawned = false;

	bool loadFromJSON(const char *jsonPath);
	void unload();
	void spawnEntities();
};

bool parseCourseDef(const char *jsonPath, CourseDef &def);
