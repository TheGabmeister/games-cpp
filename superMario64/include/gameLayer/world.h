#pragma once
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include "renderer.h"

enum class SurfaceType
{
	Normal,
	Ice,
};

enum class SlopeClass
{
	Walkable,
	Steep,
	Wall,
};

enum class Phase5ObjectType
{
	MovingPlatform,
	FallingPlatform,
	TiltingPlatform,
	OneWayPlatform,
	BreakablePlatform,
	Carriable,
};

enum CollisionLayer : unsigned int
{
	COLLISION_PLAYER = 1u << 0,
	COLLISION_ENEMY = 1u << 1,
	COLLISION_PLAYER_ATTACK = 1u << 2,
	COLLISION_COLLECTIBLE = 1u << 3,
	COLLISION_TERRAIN = 1u << 4,
	COLLISION_TRIGGER = 1u << 5,
	COLLISION_INTANGIBLE = 1u << 6,
	COLLISION_PROJECTILE = 1u << 7,
};

struct CollisionTriangle
{
	glm::vec3 v0 = {};
	glm::vec3 v1 = {};
	glm::vec3 v2 = {};
	glm::vec3 normal = {0.f, 1.f, 0.f};
	glm::vec3 minBounds = {};
	glm::vec3 maxBounds = {};
	SurfaceType surface = SurfaceType::Normal;
	SlopeClass slopeClass = SlopeClass::Walkable;
	unsigned int layer = COLLISION_TERRAIN;
};

struct CollisionHit
{
	bool hit = false;
	glm::vec3 point = {};
	glm::vec3 normal = {0.f, 1.f, 0.f};
	float y = 0.f;
	int triangleIndex = -1;
	SurfaceType surface = SurfaceType::Normal;
	SlopeClass slopeClass = SlopeClass::Walkable;
};

struct CollisionWorld
{
	float cellSize = 4.f;
	std::vector<CollisionTriangle> triangles;
	std::unordered_map<long long, std::vector<int>> grid;
	glm::vec3 minBounds = {0.f, 0.f, 0.f};
	glm::vec3 maxBounds = {0.f, 0.f, 0.f};

	void clear();
	void buildGrid();
	void queryTrianglesXZ(const glm::vec2 &center, float radius, std::vector<int> &out) const;
	bool isLoaded() const { return !triangles.empty(); }
	int triangleCount() const { return (int)triangles.size(); }
	int occupiedCellCount() const { return (int)grid.size(); }
};

struct Phase5Object
{
	Phase5ObjectType type = Phase5ObjectType::MovingPlatform;
	glm::vec3 position = {};
	glm::vec3 previousPosition = {};
	glm::vec3 startPosition = {};
	glm::vec3 endPosition = {};
	glm::vec3 velocity = {};
	glm::vec3 halfExtents = {1.f, 0.2f, 1.f};
	float timer = 0.f;
	float respawnTimer = 0.f;
	float tiltX = 0.f;
	float tiltZ = 0.f;
	bool active = true;
	bool triggered = false;
	bool carried = false;
};

struct Phase5Pole
{
	glm::vec3 base = {};
	float height = 4.f;
	float radius = 0.35f;
};

struct Phase5World
{
	std::vector<Phase5Object> objects;
	std::vector<Phase5Pole> poles;
	int carriedObject = -1;

	void clear();
};

const char *surfaceTypeName(SurfaceType surface);
const char *slopeClassName(SlopeClass slopeClass);

bool loadCollisionWorldGLB(const char *path, CollisionWorld &world);
bool queryGround(const CollisionWorld &world, const glm::vec3 &position,
	float radius, float maxAbove, float maxBelow, CollisionHit &hit);
bool queryCeiling(const CollisionWorld &world, const glm::vec3 &position,
	float radius, float headHeight, CollisionHit &hit);
void resolveHorizontalCollisions(const CollisionWorld &world, glm::vec3 &position,
	glm::vec3 &velocity, float radius, float height, float stepHeight,
	glm::vec3 *lastWallNormal = nullptr);

LineMesh createCollisionWireMesh(const CollisionWorld &world);

const char *phase5ObjectTypeName(Phase5ObjectType type);
void initPhase5TestObjects(Phase5World &world);
void updatePhase5Objects(Phase5World &world, float dt, const glm::vec3 &marioPosition, bool marioGroundPounded);
bool queryPhase5PlatformGround(Phase5World &world, const glm::vec3 &position, float previousY,
	float radius, float maxAbove, float maxBelow, CollisionHit &hit, int &objectIndex);
int findNearestPole(const Phase5World &world, const glm::vec3 &position, float grabRadius);
int findNearestCarriable(const Phase5World &world, const glm::vec3 &position, const glm::vec3 &forward, float radius);
void setCarriedObject(Phase5World &world, int objectIndex, bool carried);
void moveCarriedObject(Phase5World &world, int objectIndex, const glm::vec3 &position);
void throwCarriedObject(Phase5World &world, int objectIndex, const glm::vec3 &velocity);
LineMesh createPhase5DebugMesh(const Phase5World &world);
