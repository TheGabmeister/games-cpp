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
	glm::vec3 &velocity, float radius, float height, float stepHeight)
{
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
