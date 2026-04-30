#include "course.h"
#include <nlohmann/json.hpp>
#include "gameLayer.h"
#include <logs.h>

using json = nlohmann::json;

static EntityType parseEntityType(const std::string &s)
{
	if (s == "goomba") return EntityType::Goomba;
	if (s == "bob_omb") return EntityType::BobOmb;
	if (s == "koopa_troopa") return EntityType::KoopaTroopa;
	if (s == "boo") return EntityType::Boo;
	if (s == "king_bob_omb") return EntityType::KingBobOmb;
	if (s == "chain_chomp") return EntityType::ChainChomp;
	return EntityType::Goomba;
}

static CollectibleType parseCollectibleType(const std::string &s)
{
	if (s == "yellow_coin") return CollectibleType::YellowCoin;
	if (s == "red_coin") return CollectibleType::RedCoin;
	if (s == "blue_coin") return CollectibleType::BlueCoin;
	if (s == "spinning_heart") return CollectibleType::SpinningHeart;
	if (s == "star") return CollectibleType::Star;
	return CollectibleType::YellowCoin;
}

static Phase5ObjectType parsePlatformType(const std::string &s)
{
	if (s == "moving") return Phase5ObjectType::MovingPlatform;
	if (s == "falling") return Phase5ObjectType::FallingPlatform;
	if (s == "tilting") return Phase5ObjectType::TiltingPlatform;
	if (s == "one_way") return Phase5ObjectType::OneWayPlatform;
	if (s == "breakable") return Phase5ObjectType::BreakablePlatform;
	if (s == "carriable") return Phase5ObjectType::Carriable;
	return Phase5ObjectType::MovingPlatform;
}

static glm::vec3 parseVec3(const json &j)
{
	return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

bool parseCourseDef(const char *jsonPath, CourseDef &def)
{
	bool ok = false;
	std::string text = platform::readEntireFile(jsonPath, &ok);
	if (!ok || text.empty())
	{
		platform::log((std::string("Failed to read course JSON: ") + jsonPath).c_str());
		return false;
	}

	try
	{
		json j = json::parse(text);
		def.name = j.value("name", "Unknown Course");
		def.visualMeshPath = j.value("visualMesh", "");
		def.collisionMeshPath = j.value("collisionMesh", "");

		if (j.contains("marioSpawn"))
			def.marioSpawn = parseVec3(j["marioSpawn"]);
		def.marioSpawnAngle = j.value("marioSpawnAngle", 0.f);

		if (j.contains("stars"))
		{
			for (auto &s : j["stars"])
			{
				CourseStarDef star;
				star.starIndex = s.value("index", 0);
				star.name = s.value("name", "");
				def.stars.push_back(star);
			}
		}

		if (j.contains("enemies"))
		{
			for (auto &e : j["enemies"])
			{
				CourseEnemyDef enemy;
				enemy.type = parseEntityType(e.value("type", "goomba"));
				if (e.contains("position")) enemy.position = parseVec3(e["position"]);
				if (e.contains("patrolA")) enemy.patrolA = parseVec3(e["patrolA"]);
				else enemy.patrolA = enemy.position + glm::vec3(-3, 0, 0);
				if (e.contains("patrolB")) enemy.patrolB = parseVec3(e["patrolB"]);
				else enemy.patrolB = enemy.position + glm::vec3(3, 0, 0);
				enemy.facingAngle = e.value("facingAngle", 0.f);
				enemy.spawnRadius = e.value("spawnRadius", 50.f);
				enemy.detectionRadius = e.value("detectionRadius", 10.f);
				def.enemies.push_back(enemy);
			}
		}

		if (j.contains("platforms"))
		{
			for (auto &p : j["platforms"])
			{
				CoursePlatformDef plat;
				plat.type = parsePlatformType(p.value("type", "moving"));
				if (p.contains("position")) plat.position = parseVec3(p["position"]);
				if (p.contains("endPosition")) plat.endPosition = parseVec3(p["endPosition"]);
				else plat.endPosition = plat.position;
				if (p.contains("halfExtents")) plat.halfExtents = parseVec3(p["halfExtents"]);
				def.platforms.push_back(plat);
			}
		}

		if (j.contains("collectibles"))
		{
			for (auto &c : j["collectibles"])
			{
				CourseCollectibleDef col;
				col.type = parseCollectibleType(c.value("type", "yellow_coin"));
				if (c.contains("position")) col.position = parseVec3(c["position"]);
				def.collectibles.push_back(col);
			}
		}

		if (j.contains("poles"))
		{
			for (auto &p : j["poles"])
			{
				CoursePoleDef pole;
				if (p.contains("base")) pole.base = parseVec3(p["base"]);
				pole.height = p.value("height", 4.f);
				def.poles.push_back(pole);
			}
		}

		if (j.contains("water"))
		{
			for (auto &w : j["water"])
			{
				CourseWaterDef wv;
				if (w.contains("minBounds")) wv.minBounds = parseVec3(w["minBounds"]);
				if (w.contains("maxBounds")) wv.maxBounds = parseVec3(w["maxBounds"]);
				wv.surfaceY = w.value("surfaceY", 0.f);
				def.water.push_back(wv);
			}
		}
	}
	catch (const json::exception &e)
	{
		platform::log((std::string("JSON parse error: ") + e.what()).c_str());
		return false;
	}

	return true;
}

bool LoadedCourse::loadFromJSON(const char *jsonPath)
{
	if (!parseCourseDef(jsonPath, def))
		return false;

	std::string basePath = RESOURCES_PATH;

	if (!def.visualMeshPath.empty())
	{
		visualMesh = loadGLB((basePath + def.visualMeshPath).c_str());
		if (visualMesh.vao == 0)
		{
			platform::log("Failed to load course visual mesh");
			return false;
		}
	}

	if (!def.collisionMeshPath.empty())
	{
		if (!loadCollisionWorldGLB((basePath + def.collisionMeshPath).c_str(), collisionWorld))
		{
			platform::log("Failed to load course collision mesh");
			return false;
		}
		collisionWireMesh = createCollisionWireMesh(collisionWorld);
	}

	phase5World.clear();

	for (auto &c : def.collectibles)
	{
		Collectible col;
		col.type = c.type;
		col.position = c.position;
		col.radius = (c.type == CollectibleType::SpinningHeart) ? 1.5f :
			(c.type == CollectibleType::Star) ? 2.0f : 1.0f;
		phase5World.collectibles.push_back(col);
	}

	for (auto &p : def.poles)
		phase5World.poles.push_back({p.base, p.height, 0.35f});

	for (auto &w : def.water)
		phase5World.waterVolumes.push_back({w.minBounds, w.maxBounds, w.surfaceY});

	for (auto &p : def.platforms)
	{
		Phase5Object obj;
		obj.type = p.type;
		obj.position = p.position;
		obj.startPosition = p.position;
		obj.endPosition = p.endPosition;
		obj.halfExtents = p.halfExtents;
		phase5World.objects.push_back(obj);
	}

	redCoinsCollected = 0;
	hundredCoinStarSpawned = false;
	loaded = true;
	return true;
}

void LoadedCourse::spawnEntities()
{
	entityManager.clear();
	for (auto &enemyDef : def.enemies)
	{
		std::unique_ptr<Entity> entity;
		switch (enemyDef.type)
		{
		case EntityType::Goomba: entity = std::make_unique<GoombaEntity>(); break;
		case EntityType::BobOmb: entity = std::make_unique<BobOmbEntity>(); break;
		case EntityType::KoopaTroopa: entity = std::make_unique<KoopaTroopaEntity>(); break;
		case EntityType::Boo: entity = std::make_unique<BooEntity>(); break;
		default: entity = std::make_unique<GoombaEntity>(); break;
		}
		entity->spawnPosition = enemyDef.position;
		entity->position = enemyDef.position;
		entity->patrolPointA = enemyDef.patrolA;
		entity->patrolPointB = enemyDef.patrolB;
		entity->facingAngle = enemyDef.facingAngle;
		entity->spawnRadius = enemyDef.spawnRadius;
		entity->despawnRadius = enemyDef.spawnRadius + 10.f;
		entity->detectionRadius = enemyDef.detectionRadius;
		entityManager.addEntity(std::move(entity));
	}
}

void LoadedCourse::unload()
{
	if (visualMesh.vao) destroyMesh(visualMesh);
	collisionWorld.clear();
	if (collisionWireMesh.vao) destroyLineMesh(collisionWireMesh);
	phase5World.clear();
	entityManager.clear();
	def = {};
	loaded = false;
}
