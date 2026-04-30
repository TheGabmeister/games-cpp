#define GLM_ENABLE_EXPERIMENTAL
#include "world.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <platformTools.h>
#include <logs.h>
#include <glm/gtc/type_ptr.hpp>

#include <cgltf/cgltf.h>

static constexpr float WALKABLE_MIN_Y = 0.8660254f; // cos(30)
static constexpr float WALL_MAX_Y = 0.6427876f;     // cos(50)

static long long cellKey(int x, int z)
{
	return (static_cast<long long>(x) << 32) ^ static_cast<unsigned int>(z);
}

static int cellCoord(float value, float cellSize)
{
	return (int)std::floor(value / cellSize);
}

static std::string lowerString(const char *text)
{
	std::string out = text ? text : "";
	for (char &c : out)
		c = (char)std::tolower((unsigned char)c);
	return out;
}

static SurfaceType surfaceFromNames(const cgltf_node *node, const cgltf_mesh *mesh, const cgltf_primitive &prim)
{
	std::string names;
	names += lowerString(node ? node->name : nullptr);
	names += " ";
	names += lowerString(mesh ? mesh->name : nullptr);
	if (prim.material)
	{
		names += " ";
		names += lowerString(prim.material->name);
	}
	return names.find("ice") != std::string::npos ? SurfaceType::Ice : SurfaceType::Normal;
}

static SlopeClass classifySlope(const glm::vec3 &normal)
{
	float y = std::abs(normal.y);
	if (y >= WALKABLE_MIN_Y) return SlopeClass::Walkable;
	if (y >= WALL_MAX_Y) return SlopeClass::Steep;
	return SlopeClass::Wall;
}

static CollisionTriangle makeTriangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, SurfaceType surface)
{
	CollisionTriangle tri;
	tri.v0 = a;
	tri.v1 = b;
	tri.v2 = c;
	tri.normal = glm::cross(b - a, c - a);
	float len = glm::length(tri.normal);
	if (len > 0.00001f)
		tri.normal /= len;
	if (tri.normal.y < 0.f)
		tri.normal = -tri.normal;

	tri.minBounds = glm::min(a, glm::min(b, c));
	tri.maxBounds = glm::max(a, glm::max(b, c));
	tri.surface = surface;
	tri.slopeClass = classifySlope(tri.normal);
	return tri;
}

void CollisionWorld::clear()
{
	triangles.clear();
	grid.clear();
	minBounds = {};
	maxBounds = {};
}

void CollisionWorld::buildGrid()
{
	grid.clear();
	if (triangles.empty())
		return;

	minBounds = triangles[0].minBounds;
	maxBounds = triangles[0].maxBounds;

	for (int i = 0; i < (int)triangles.size(); i++)
	{
		const CollisionTriangle &tri = triangles[i];
		minBounds = glm::min(minBounds, tri.minBounds);
		maxBounds = glm::max(maxBounds, tri.maxBounds);

		int minX = cellCoord(tri.minBounds.x, cellSize);
		int maxX = cellCoord(tri.maxBounds.x, cellSize);
		int minZ = cellCoord(tri.minBounds.z, cellSize);
		int maxZ = cellCoord(tri.maxBounds.z, cellSize);

		for (int x = minX; x <= maxX; x++)
			for (int z = minZ; z <= maxZ; z++)
				grid[cellKey(x, z)].push_back(i);
	}
}

void CollisionWorld::queryTrianglesXZ(const glm::vec2 &center, float radius, std::vector<int> &out) const
{
	out.clear();
	int minX = cellCoord(center.x - radius, cellSize);
	int maxX = cellCoord(center.x + radius, cellSize);
	int minZ = cellCoord(center.y - radius, cellSize);
	int maxZ = cellCoord(center.y + radius, cellSize);

	for (int x = minX; x <= maxX; x++)
	{
		for (int z = minZ; z <= maxZ; z++)
		{
			auto it = grid.find(cellKey(x, z));
			if (it == grid.end())
				continue;

			for (int triIndex : it->second)
			{
				if (std::find(out.begin(), out.end(), triIndex) == out.end())
					out.push_back(triIndex);
			}
		}
	}
}

const char *surfaceTypeName(SurfaceType surface)
{
	switch (surface)
	{
	case SurfaceType::Normal: return "Normal";
	case SurfaceType::Ice: return "Ice";
	}
	return "Unknown";
}

const char *slopeClassName(SlopeClass slopeClass)
{
	switch (slopeClass)
	{
	case SlopeClass::Walkable: return "Walkable";
	case SlopeClass::Steep: return "Steep";
	case SlopeClass::Wall: return "Wall";
	}
	return "Unknown";
}

static bool pointInTriangleXZ(const glm::vec2 &p, const CollisionTriangle &tri)
{
	glm::vec2 a(tri.v0.x, tri.v0.z);
	glm::vec2 b(tri.v1.x, tri.v1.z);
	glm::vec2 c(tri.v2.x, tri.v2.z);
	float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
	if (std::abs(area) < 0.00001f)
		return false;

	float w0 = ((b.x - p.x) * (c.y - p.y) - (b.y - p.y) * (c.x - p.x)) / area;
	float w1 = ((c.x - p.x) * (a.y - p.y) - (c.y - p.y) * (a.x - p.x)) / area;
	float w2 = 1.f - w0 - w1;
	constexpr float eps = -0.001f;
	return w0 >= eps && w1 >= eps && w2 >= eps;
}

static bool triangleYAtXZ(const CollisionTriangle &tri, const glm::vec2 &xz, float &y)
{
	if (std::abs(tri.normal.y) < 0.0001f)
		return false;
	if (!pointInTriangleXZ(xz, tri))
		return false;

	y = tri.v0.y - (tri.normal.x * (xz.x - tri.v0.x) + tri.normal.z * (xz.y - tri.v0.z)) / tri.normal.y;
	return true;
}

bool queryGround(const CollisionWorld &world, const glm::vec3 &position,
	float radius, float maxAbove, float maxBelow, CollisionHit &hit)
{
	std::vector<int> candidates;
	world.queryTrianglesXZ({position.x, position.z}, radius, candidates);

	bool found = false;
	float bestY = -std::numeric_limits<float>::max();
	for (int triIndex : candidates)
	{
		const CollisionTriangle &tri = world.triangles[triIndex];
		if (tri.slopeClass == SlopeClass::Wall)
			continue;

		float y = 0.f;
		if (!triangleYAtXZ(tri, {position.x, position.z}, y))
			continue;
		if (y > position.y + maxAbove || y < position.y - maxBelow)
			continue;
		if (!found || y > bestY)
		{
			found = true;
			bestY = y;
			hit.hit = true;
			hit.y = y;
			hit.point = {position.x, y, position.z};
			hit.normal = tri.normal;
			hit.triangleIndex = triIndex;
			hit.surface = tri.surface;
			hit.slopeClass = tri.slopeClass;
		}
	}
	return found;
}

bool queryCeiling(const CollisionWorld &world, const glm::vec3 &position,
	float radius, float headHeight, CollisionHit &hit)
{
	std::vector<int> candidates;
	world.queryTrianglesXZ({position.x, position.z}, radius, candidates);

	float headY = position.y + headHeight;
	bool found = false;
	float bestY = std::numeric_limits<float>::max();
	for (int triIndex : candidates)
	{
		const CollisionTriangle &tri = world.triangles[triIndex];
		if (tri.slopeClass == SlopeClass::Wall)
			continue;

		float y = 0.f;
		if (!triangleYAtXZ(tri, {position.x, position.z}, y))
			continue;
		if (y < headY - 0.02f || y > headY + 0.35f)
			continue;
		if (!found || y < bestY)
		{
			found = true;
			bestY = y;
			hit.hit = true;
			hit.y = y;
			hit.point = {position.x, y, position.z};
			hit.normal = tri.normal;
			hit.triangleIndex = triIndex;
			hit.surface = tri.surface;
			hit.slopeClass = tri.slopeClass;
		}
	}
	return found;
}

void resolveHorizontalCollisions(const CollisionWorld &world, glm::vec3 &position,
	glm::vec3 &velocity, float radius, float height, float stepHeight,
	glm::vec3 *lastWallNormal)
{
	if (lastWallNormal)
		*lastWallNormal = {0.f, 0.f, 0.f};

	std::vector<int> candidates;
	world.queryTrianglesXZ({position.x, position.z}, radius + 0.5f, candidates);

	for (int pass = 0; pass < 3; pass++)
	{
		bool pushed = false;
		for (int triIndex : candidates)
		{
			const CollisionTriangle &tri = world.triangles[triIndex];
			if (tri.slopeClass != SlopeClass::Wall)
				continue;
			if (tri.maxBounds.y <= position.y + stepHeight + 0.03f)
				continue;
			if (tri.maxBounds.y < position.y + 0.1f || tri.minBounds.y > position.y + height)
				continue;

			float dist = glm::dot(position - tri.v0, tri.normal);
			if (std::abs(dist) >= radius)
				continue;

			glm::vec3 pushDir = tri.normal * (dist >= 0.f ? 1.f : -1.f);
			pushDir.y = 0.f;
			float len = glm::length(pushDir);
			if (len < 0.0001f)
				continue;
			pushDir /= len;

			float push = radius - std::abs(dist);
			position += pushDir * (push + 0.001f);
			if (lastWallNormal)
				*lastWallNormal = pushDir;

			glm::vec3 hVel(velocity.x, 0.f, velocity.z);
			float into = glm::dot(hVel, pushDir);
			if (into < 0.f)
			{
				hVel -= pushDir * into;
				velocity.x = hVel.x;
				velocity.z = hVel.z;
			}
			pushed = true;
		}

		if (!pushed)
			break;
	}
}

bool loadCollisionWorldGLB(const char *path, CollisionWorld &world)
{
	world.clear();

	cgltf_options options = {};
	cgltf_data *data = nullptr;
	cgltf_result result = cgltf_parse_file(&options, path, &data);
	if (result != cgltf_result_success)
	{
		platform::log((std::string("Failed to parse collision glTF: ") + path).c_str());
		return false;
	}

	result = cgltf_load_buffers(&options, data, path);
	if (result != cgltf_result_success)
	{
		platform::log((std::string("Failed to load collision glTF buffers: ") + path).c_str());
		cgltf_free(data);
		return false;
	}

	for (cgltf_size n = 0; n < data->nodes_count; n++)
	{
		cgltf_node *node = &data->nodes[n];
		if (!node->mesh)
			continue;

		float rawMatrix[16];
		cgltf_node_transform_world(node, rawMatrix);
		glm::mat4 transform = glm::make_mat4(rawMatrix);

		for (cgltf_size p = 0; p < node->mesh->primitives_count; p++)
		{
			cgltf_primitive &prim = node->mesh->primitives[p];
			if (prim.type != cgltf_primitive_type_triangles)
				continue;

			cgltf_accessor *posAccessor = nullptr;
			for (cgltf_size a = 0; a < prim.attributes_count; a++)
				if (prim.attributes[a].type == cgltf_attribute_type_position)
					posAccessor = prim.attributes[a].data;

			if (!posAccessor)
				continue;

			std::vector<glm::vec3> vertices(posAccessor->count);
			for (cgltf_size i = 0; i < posAccessor->count; i++)
			{
				glm::vec3 p0;
				cgltf_accessor_read_float(posAccessor, i, &p0.x, 3);
				vertices[i] = glm::vec3(transform * glm::vec4(p0, 1.f));
			}

			std::vector<unsigned int> indices;
			if (prim.indices)
			{
				indices.resize(prim.indices->count);
				for (cgltf_size i = 0; i < prim.indices->count; i++)
					indices[i] = (unsigned int)cgltf_accessor_read_index(prim.indices, i);
			}
			else
			{
				indices.resize(vertices.size());
				for (size_t i = 0; i < vertices.size(); i++)
					indices[i] = (unsigned int)i;
			}

			SurfaceType surface = surfaceFromNames(node, node->mesh, prim);
			for (size_t i = 0; i + 2 < indices.size(); i += 3)
			{
				unsigned int i0 = indices[i];
				unsigned int i1 = indices[i + 1];
				unsigned int i2 = indices[i + 2];
				if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
					continue;

				CollisionTriangle tri = makeTriangle(vertices[i0], vertices[i1], vertices[i2], surface);
				if (glm::length(glm::cross(tri.v1 - tri.v0, tri.v2 - tri.v0)) > 0.00001f)
					world.triangles.push_back(tri);
			}
		}
	}

	cgltf_free(data);
	world.buildGrid();

	platform::log((std::string("Loaded collision GLB: ") + path +
		" (" + std::to_string(world.triangleCount()) + " tris, " +
		std::to_string(world.occupiedCellCount()) + " grid cells)").c_str());
	return world.isLoaded();
}

LineMesh createCollisionWireMesh(const CollisionWorld &world)
{
	std::vector<LineVertex> vertices;
	vertices.reserve(world.triangles.size() * 6);

	for (const CollisionTriangle &tri : world.triangles)
	{
		glm::vec3 color(0.3f, 0.9f, 0.3f);
		if (tri.surface == SurfaceType::Ice)
			color = {0.25f, 0.85f, 1.f};
		else if (tri.slopeClass == SlopeClass::Steep)
			color = {1.f, 0.8f, 0.2f};
		else if (tri.slopeClass == SlopeClass::Wall)
			color = {1.f, 0.2f, 0.2f};

		vertices.push_back({tri.v0, color});
		vertices.push_back({tri.v1, color});
		vertices.push_back({tri.v1, color});
		vertices.push_back({tri.v2, color});
		vertices.push_back({tri.v2, color});
		vertices.push_back({tri.v0, color});
	}

	return createLineMesh(vertices);
}

void Phase5World::clear()
{
	objects.clear();
	poles.clear();
	collectibles.clear();
	enemies.clear();
	waterVolumes.clear();
	carriedObject = -1;
}

const char *phase5ObjectTypeName(Phase5ObjectType type)
{
	switch (type)
	{
	case Phase5ObjectType::MovingPlatform: return "MovingPlatform";
	case Phase5ObjectType::FallingPlatform: return "FallingPlatform";
	case Phase5ObjectType::TiltingPlatform: return "TiltingPlatform";
	case Phase5ObjectType::OneWayPlatform: return "OneWayPlatform";
	case Phase5ObjectType::BreakablePlatform: return "BreakablePlatform";
	case Phase5ObjectType::Carriable: return "Carriable";
	}
	return "Unknown";
}

static Phase5Object makeObject(Phase5ObjectType type, glm::vec3 position, glm::vec3 halfExtents)
{
	Phase5Object object;
	object.type = type;
	object.position = position;
	object.previousPosition = position;
	object.startPosition = position;
	object.endPosition = position;
	object.halfExtents = halfExtents;
	return object;
}

void initPhase5TestObjects(Phase5World &world)
{
	world.clear();

	Phase5Object moving = makeObject(Phase5ObjectType::MovingPlatform, {-12.f, 1.f, 13.f}, {1.8f, 0.18f, 1.8f});
	moving.startPosition = {-12.f, 1.f, 13.f};
	moving.endPosition = {-4.f, 1.f, 13.f};
	world.objects.push_back(moving);

	world.objects.push_back(makeObject(Phase5ObjectType::FallingPlatform, {-2.f, 1.5f, 13.f}, {1.6f, 0.18f, 1.6f}));
	world.objects.push_back(makeObject(Phase5ObjectType::TiltingPlatform, {4.f, 1.2f, 13.f}, {2.2f, 0.18f, 2.2f}));
	world.objects.push_back(makeObject(Phase5ObjectType::OneWayPlatform, {10.f, 2.2f, 13.f}, {2.f, 0.12f, 2.f}));
	world.objects.push_back(makeObject(Phase5ObjectType::BreakablePlatform, {15.f, 1.4f, 13.f}, {1.7f, 0.18f, 1.7f}));
	world.objects.push_back(makeObject(Phase5ObjectType::Carriable, {-5.f, 0.5f, 5.f}, {0.45f, 0.45f, 0.45f}));

	world.poles.push_back({{-12.f, 0.f, 5.f}, 4.f, 0.35f});
}

void updatePhase5Objects(Phase5World &world, float dt, const glm::vec3 &marioPosition, bool marioGroundPounded)
{
	for (int i = 0; i < (int)world.objects.size(); i++)
	{
		Phase5Object &object = world.objects[i];
		object.previousPosition = object.position;

		if (!object.active)
		{
			object.respawnTimer -= dt;
			if (object.respawnTimer <= 0.f)
			{
				object.active = true;
				object.triggered = false;
				object.timer = 0.f;
				object.position = object.startPosition;
				object.velocity = {};
			}
			continue;
		}

		if (object.carried)
			continue;

		switch (object.type)
		{
		case Phase5ObjectType::MovingPlatform:
		{
			object.timer += dt;
			float t = (std::sin(object.timer * 1.2f) + 1.f) * 0.5f;
			object.position = glm::mix(object.startPosition, object.endPosition, t);
			object.velocity = (object.position - object.previousPosition) / std::max(dt, 0.0001f);
			break;
		}
		case Phase5ObjectType::FallingPlatform:
			if (object.triggered)
			{
				object.timer += dt;
				if (object.timer > 0.4f)
				{
					object.velocity.y -= 18.f * dt;
					object.position += object.velocity * dt;
					if (object.position.y < -8.f)
					{
						object.active = false;
						object.respawnTimer = 3.f;
					}
				}
			}
			break;
		case Phase5ObjectType::TiltingPlatform:
		{
			glm::vec3 local = marioPosition - object.position;
			object.tiltX = std::clamp(local.z / object.halfExtents.z, -1.f, 1.f) * 25.f;
			object.tiltZ = std::clamp(-local.x / object.halfExtents.x, -1.f, 1.f) * 25.f;
			break;
		}
		case Phase5ObjectType::BreakablePlatform:
			if (marioGroundPounded && std::abs(marioPosition.x - object.position.x) <= object.halfExtents.x &&
				std::abs(marioPosition.z - object.position.z) <= object.halfExtents.z &&
				std::abs(marioPosition.y - (object.position.y + object.halfExtents.y)) < 0.35f)
			{
				object.active = false;
				object.respawnTimer = 3.f;
			}
			break;
		case Phase5ObjectType::Carriable:
			object.velocity.y -= 20.f * dt;
			object.position += object.velocity * dt;
			if (object.position.y < object.halfExtents.y)
			{
				object.position.y = object.halfExtents.y;
				object.velocity.y = 0.f;
				object.velocity.x *= 0.9f;
				object.velocity.z *= 0.9f;
			}
			break;
		case Phase5ObjectType::OneWayPlatform:
			break;
		}
	}
}

bool queryPhase5PlatformGround(Phase5World &world, const glm::vec3 &position, float previousY,
	float radius, float maxAbove, float maxBelow, CollisionHit &hit, int &objectIndex)
{
	bool found = false;
	float bestY = -std::numeric_limits<float>::max();
	objectIndex = -1;

	for (int i = 0; i < (int)world.objects.size(); i++)
	{
		Phase5Object &object = world.objects[i];
		if (!object.active || object.type == Phase5ObjectType::Carriable || object.carried)
			continue;

		float topY = object.position.y + object.halfExtents.y;
		if (object.type == Phase5ObjectType::OneWayPlatform && previousY < topY - 0.02f)
			continue;
		if (std::abs(position.x - object.position.x) > object.halfExtents.x + radius ||
			std::abs(position.z - object.position.z) > object.halfExtents.z + radius)
			continue;
		if (topY > position.y + maxAbove || topY < position.y - maxBelow)
			continue;

		if (!found || topY > bestY)
		{
			found = true;
			bestY = topY;
			objectIndex = i;
			hit.hit = true;
			hit.y = topY;
			hit.point = {position.x, topY, position.z};
			hit.normal = {0.f, 1.f, 0.f};
			hit.surface = SurfaceType::Normal;
			hit.slopeClass = SlopeClass::Walkable;
		}
	}

	if (found && objectIndex >= 0 && world.objects[objectIndex].type == Phase5ObjectType::FallingPlatform)
		world.objects[objectIndex].triggered = true;

	return found;
}

int findNearestPole(const Phase5World &world, const glm::vec3 &position, float grabRadius)
{
	int best = -1;
	float bestDist = grabRadius;
	for (int i = 0; i < (int)world.poles.size(); i++)
	{
		const Phase5Pole &pole = world.poles[i];
		if (position.y < pole.base.y || position.y > pole.base.y + pole.height)
			continue;

		float dist = glm::length(glm::vec2(position.x - pole.base.x, position.z - pole.base.z)) - pole.radius;
		if (dist < bestDist)
		{
			best = i;
			bestDist = dist;
		}
	}
	return best;
}

int findNearestCarriable(const Phase5World &world, const glm::vec3 &position, const glm::vec3 &forward, float radius)
{
	int best = -1;
	float bestScore = radius;
	for (int i = 0; i < (int)world.objects.size(); i++)
	{
		const Phase5Object &object = world.objects[i];
		if (!object.active || object.carried || object.type != Phase5ObjectType::Carriable)
			continue;

		glm::vec3 toObject = object.position - position;
		toObject.y = 0.f;
		float dist = glm::length(toObject);
		if (dist > radius || dist < 0.001f)
			continue;
		if (glm::dot(glm::normalize(toObject), forward) < 0.2f)
			continue;
		if (dist < bestScore)
		{
			best = i;
			bestScore = dist;
		}
	}
	return best;
}

void setCarriedObject(Phase5World &world, int objectIndex, bool carried)
{
	if (objectIndex < 0 || objectIndex >= (int)world.objects.size())
		return;
	world.objects[objectIndex].carried = carried;
	world.objects[objectIndex].velocity = {};
	world.carriedObject = carried ? objectIndex : -1;
}

void moveCarriedObject(Phase5World &world, int objectIndex, const glm::vec3 &position)
{
	if (objectIndex < 0 || objectIndex >= (int)world.objects.size())
		return;
	world.objects[objectIndex].position = position;
}

void throwCarriedObject(Phase5World &world, int objectIndex, const glm::vec3 &velocity)
{
	if (objectIndex < 0 || objectIndex >= (int)world.objects.size())
		return;
	world.objects[objectIndex].carried = false;
	world.objects[objectIndex].velocity = velocity;
	if (world.carriedObject == objectIndex)
		world.carriedObject = -1;
}

static void addBoxLines(std::vector<LineVertex> &vertices, glm::vec3 center, glm::vec3 halfExtents, glm::vec3 color)
{
	glm::vec3 p[8] = {
		center + glm::vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z),
		center + glm::vec3( halfExtents.x, -halfExtents.y, -halfExtents.z),
		center + glm::vec3( halfExtents.x, -halfExtents.y,  halfExtents.z),
		center + glm::vec3(-halfExtents.x, -halfExtents.y,  halfExtents.z),
		center + glm::vec3(-halfExtents.x,  halfExtents.y, -halfExtents.z),
		center + glm::vec3( halfExtents.x,  halfExtents.y, -halfExtents.z),
		center + glm::vec3( halfExtents.x,  halfExtents.y,  halfExtents.z),
		center + glm::vec3(-halfExtents.x,  halfExtents.y,  halfExtents.z),
	};
	int edges[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
	for (auto &edge : edges)
	{
		vertices.push_back({p[edge[0]], color});
		vertices.push_back({p[edge[1]], color});
	}
}

LineMesh createPhase5DebugMesh(const Phase5World &world)
{
	std::vector<LineVertex> vertices;
	for (const Phase5Object &object : world.objects)
	{
		if (!object.active)
			continue;
		glm::vec3 color(0.9f, 0.9f, 0.2f);
		if (object.type == Phase5ObjectType::Carriable) color = {1.f, 0.45f, 0.2f};
		if (object.type == Phase5ObjectType::OneWayPlatform) color = {0.4f, 0.8f, 1.f};
		if (object.type == Phase5ObjectType::BreakablePlatform) color = {1.f, 0.25f, 0.25f};
		addBoxLines(vertices, object.position, object.halfExtents, color);
	}

	for (const Phase5Pole &pole : world.poles)
	{
		glm::vec3 bottom = pole.base;
		glm::vec3 top = pole.base + glm::vec3(0.f, pole.height, 0.f);
		glm::vec3 color(0.8f, 0.5f, 0.25f);
		vertices.push_back({bottom, color});
		vertices.push_back({top, color});
	}

	return createLineMesh(vertices);
}

// ---- Collectibles ----

const char *collectibleTypeName(CollectibleType type)
{
	switch (type)
	{
	case CollectibleType::YellowCoin: return "YellowCoin";
	case CollectibleType::RedCoin: return "RedCoin";
	case CollectibleType::BlueCoin: return "BlueCoin";
	case CollectibleType::SpinningHeart: return "SpinningHeart";
	}
	return "Unknown";
}

static Collectible makeCoin(CollectibleType type, glm::vec3 position)
{
	Collectible c;
	c.type = type;
	c.position = position;
	c.radius = 1.0f;
	return c;
}

void initPhase6Collectibles(Phase5World &world)
{
	// Yellow coins scattered around the level
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {3.f, 0.8f, 3.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {6.f, 0.8f, 3.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {9.f, 0.8f, 3.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {3.f, 0.8f, -3.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {6.f, 0.8f, -3.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {-3.f, 0.8f, 8.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {0.f, 0.8f, 8.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {3.f, 0.8f, 8.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {-8.f, 0.8f, 0.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::YellowCoin, {-8.f, 0.8f, 3.f}));

	// Red coins on elevated areas
	world.collectibles.push_back(makeCoin(CollectibleType::RedCoin, {10.f, 3.0f, 13.f}));
	world.collectibles.push_back(makeCoin(CollectibleType::RedCoin, {-12.f, 5.0f, 5.f}));

	// Blue coin on high platform
	world.collectibles.push_back(makeCoin(CollectibleType::BlueCoin, {4.f, 2.0f, 13.f}));

	// Spinning heart near start
	Collectible heart;
	heart.type = CollectibleType::SpinningHeart;
	heart.position = {-3.f, 0.8f, 0.f};
	heart.radius = 1.5f;
	world.collectibles.push_back(heart);
}

void updateCollectibles(Phase5World &world, float dt)
{
	for (Collectible &c : world.collectibles)
	{
		if (!c.active && c.type == CollectibleType::SpinningHeart)
			c.active = true;

		c.bobTimer += dt;
		c.spinAngle += 180.f * dt;
		if (c.spinAngle > 360.f) c.spinAngle -= 360.f;
	}
}

int checkCollectiblePickup(Phase5World &world, const glm::vec3 &marioPos)
{
	for (int i = 0; i < (int)world.collectibles.size(); i++)
	{
		Collectible &c = world.collectibles[i];
		if (!c.active) continue;
		if (c.type == CollectibleType::SpinningHeart) continue;

		float dist = glm::length(marioPos - c.position);
		if (dist < c.radius)
		{
			c.active = false;
			return i;
		}
	}
	return -1;
}

bool checkSpinningHeart(const Phase5World &world, const glm::vec3 &marioPos)
{
	for (const Collectible &c : world.collectibles)
	{
		if (!c.active || c.type != CollectibleType::SpinningHeart) continue;
		float dist = glm::length(marioPos - c.position);
		if (dist < c.radius)
			return true;
	}
	return false;
}

void initTestEnemies(Phase5World &world)
{
	world.enemies.push_back({{5.f, 0.5f, -5.f}, 0.8f, true});
	world.enemies.push_back({{-6.f, 0.5f, 6.f}, 0.8f, true});
	world.enemies.push_back({{8.f, 0.5f, 8.f}, 0.8f, true});
}

int checkEnemyContact(const Phase5World &world, const glm::vec3 &marioPos, float marioRadius)
{
	for (int i = 0; i < (int)world.enemies.size(); i++)
	{
		const TestEnemy &e = world.enemies[i];
		if (!e.active) continue;
		float dist = glm::length(marioPos - e.position);
		if (dist < e.radius + marioRadius)
			return i;
	}
	return -1;
}

// ---- Water ----

void initPhase7Water(Phase5World &world)
{
	WaterVolume pool;
	pool.minBounds = {-20.f, -5.f, 18.f};
	pool.maxBounds = {20.f, 1.5f, 35.f};
	pool.surfaceY = 1.5f;
	world.waterVolumes.push_back(pool);
}

int findWaterVolume(const Phase5World &world, const glm::vec3 &position)
{
	for (int i = 0; i < (int)world.waterVolumes.size(); i++)
	{
		const WaterVolume &wv = world.waterVolumes[i];
		if (position.x >= wv.minBounds.x && position.x <= wv.maxBounds.x &&
			position.z >= wv.minBounds.z && position.z <= wv.maxBounds.z &&
			position.y <= wv.surfaceY && position.y >= wv.minBounds.y)
			return i;
	}
	return -1;
}
