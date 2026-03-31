#include "itemSystems.h"
#include "trackSystems.h"
#include "gameConfig.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

ItemType rollRandomItem(int place, int totalKarts)
{
	// Weighted by position: worse position = better items
	float ratio = (totalKarts > 1) ? static_cast<float>(place - 1) / (totalKarts - 1) : 0.f;
	// Simple pseudo-random using a static counter
	static unsigned int itemSeed = 0;
	itemSeed = itemSeed * 1103515245 + 12345;
	float roll = static_cast<float>((itemSeed >> 16) & 0x7FFF) / 32767.f;

	if (ratio > 0.6f)
	{
		// Back of pack: more mushrooms and red shells
		if (roll < 0.35f) { return ItemType::Mushroom; }
		if (roll < 0.55f) { return ItemType::RedShell; }
		if (roll < 0.80f) { return ItemType::GreenShell; }
		return ItemType::Banana;
	}
	else if (ratio > 0.3f)
	{
		// Mid pack
		if (roll < 0.25f) { return ItemType::Mushroom; }
		if (roll < 0.45f) { return ItemType::GreenShell; }
		if (roll < 0.65f) { return ItemType::RedShell; }
		return ItemType::Banana;
	}
	else
	{
		// Front of pack: more bananas and green shells
		if (roll < 0.35f) { return ItemType::Banana; }
		if (roll < 0.65f) { return ItemType::GreenShell; }
		if (roll < 0.80f) { return ItemType::Mushroom; }
		return ItemType::RedShell;
	}
}

int findKartAhead(const GameState &game, int kartIndex)
{
	for (int r = 0; r < static_cast<int>(game.race.ranking.size()); ++r)
	{
		if (game.race.ranking[r] == kartIndex && r > 0)
		{
			return game.race.ranking[r - 1];
		}
	}
	return -1;
}

void useItem(GameState &game, int kartIndex)
{
	KartState &kart = game.karts[kartIndex];
	if (kart.heldItem == ItemType::None) { return; }

	ItemType item = kart.heldItem;
	kart.heldItem = ItemType::None;
	game.events.push({GameEventType::ItemUsed, kartIndex});

	glm::vec3 forward = {std::cos(kart.heading), 0.f, std::sin(kart.heading)};

	if (item == ItemType::Mushroom)
	{
		kart.boostTimer = MUSHROOM_BOOST_DURATION;
		kart.speed += BOOST_EXTRA_SPEED * 0.5f;
	}
	else if (item == ItemType::Banana)
	{
		Hazard h = {};
		h.position = kart.position - forward * 3.f;
		h.position.y = 0.3f;
		h.ownerKart = kartIndex;
		h.alive = true;
		game.hazards.push_back(h);
	}
	else if (item == ItemType::GreenShell)
	{
		Projectile p = {};
		p.position = kart.position + forward * 2.f;
		p.position.y = 0.4f;
		p.velocity = forward * GREEN_SHELL_SPEED;
		p.heading = kart.heading;
		p.type = ItemType::GreenShell;
		p.ownerKart = kartIndex;
		p.lifetime = GREEN_SHELL_LIFETIME;
		p.bounceCount = 0;
		p.alive = true;
		game.projectiles.push_back(p);
	}
	else if (item == ItemType::RedShell)
	{
		int target = findKartAhead(game, kartIndex);
		Projectile p = {};
		p.position = kart.position + forward * 2.f;
		p.position.y = 0.4f;
		p.velocity = forward * RED_SHELL_SPEED;
		p.heading = kart.heading;
		p.type = ItemType::RedShell;
		p.ownerKart = kartIndex;
		p.targetKart = target;
		p.lifetime = RED_SHELL_LIFETIME;
		p.alive = true;
		game.projectiles.push_back(p);
	}
}

void updateItemSystems(GameState &game, float deltaTime)
{
	// Item box respawn
	for (ItemBox &box : game.track.itemBoxes)
	{
		if (!box.active)
		{
			box.respawnTimer -= deltaTime;
			if (box.respawnTimer <= 0.f)
			{
				box.active = true;
				box.respawnTimer = 0.f;
			}
		}
	}

	// Item box pickup
	if (game.race.phase == RacePhase::Racing)
	{
		for (int i = 0; i < static_cast<int>(game.karts.size()); ++i)
		{
			KartState &kart = game.karts[i];
			if (kart.heldItem != ItemType::None || kart.spinoutTimer > 0.f) { continue; }

			for (ItemBox &box : game.track.itemBoxes)
			{
				if (!box.active) { continue; }
				float dist = glm::length(kart.position - box.position);
				if (dist < ITEM_BOX_RADIUS + KART_HIT_RADIUS)
				{
					int place = 1;
					for (int r = 0; r < static_cast<int>(game.race.ranking.size()); ++r)
					{
						if (game.race.ranking[r] == i) { place = r + 1; break; }
					}
					kart.heldItem = rollRandomItem(place, static_cast<int>(game.karts.size()));
					box.active = false;
					box.respawnTimer = ITEM_BOX_RESPAWN_TIME;
					game.events.push({GameEventType::ItemPickedUp, i});
					break;
				}
			}
		}
	}

	// Player item use
	for (int i = 0; i < static_cast<int>(game.karts.size()); ++i)
	{
		KartState &kart = game.karts[i];
		if (kart.controlType == KartControlType::Player)
		{
			if (kart.input.useItemPressed && kart.heldItem != ItemType::None
				&& kart.spinoutTimer <= 0.f && game.race.phase == RacePhase::Racing)
			{
				useItem(game, i);
			}
		}
		else
		{
			// AI item use: use after a delay
			if (kart.heldItem != ItemType::None && game.race.phase == RacePhase::Racing
				&& kart.spinoutTimer <= 0.f)
			{
				kart.aiItemTimer += deltaTime;
				if (kart.aiItemTimer >= AI_ITEM_USE_DELAY)
				{
					useItem(game, i);
					kart.aiItemTimer = 0.f;
				}
			}
			else
			{
				kart.aiItemTimer = 0.f;
			}
		}
	}

	// Projectile update
	for (Projectile &proj : game.projectiles)
	{
		if (!proj.alive) { continue; }

		proj.lifetime -= deltaTime;
		if (proj.lifetime <= 0.f) { proj.alive = false; continue; }

		// Red shell homing
		if (proj.type == ItemType::RedShell && proj.targetKart >= 0
			&& proj.targetKart < static_cast<int>(game.karts.size()))
		{
			const KartState &target = game.karts[proj.targetKart];
			glm::vec3 toTarget = target.position - proj.position;
			toTarget.y = 0.f;
			float dist = glm::length(toTarget);
			if (dist > 0.5f)
			{
				glm::vec3 desiredDir = toTarget / dist;
				glm::vec3 currentDir = glm::normalize(proj.velocity);
				glm::vec3 newDir = currentDir + (desiredDir - currentDir)
					* glm::clamp(RED_SHELL_HOMING_RATE * deltaTime, 0.f, 1.f);
				float len = glm::length(newDir);
				if (len > 0.001f) { newDir /= len; }
				proj.velocity = newDir * RED_SHELL_SPEED;
			}
		}

		proj.position += proj.velocity * deltaTime;
		proj.position.y = 0.4f;

		// Wall collision for projectiles
		TrackQuery pq = queryTrackPosition(proj.position, game.track);
		if (pq.lateralDistance > game.track.wallHalfWidth)
		{
			if (proj.type == ItemType::GreenShell && proj.bounceCount < GREEN_SHELL_MAX_BOUNCES)
			{
				proj.position = pq.closestPoint + pq.normal * (game.track.wallHalfWidth - 0.1f);
				proj.position.y = 0.4f;
				// Reflect velocity off wall normal
				float dotVN = glm::dot(proj.velocity, pq.normal);
				proj.velocity -= 2.f * dotVN * pq.normal;
				proj.bounceCount++;
			}
			else
			{
				proj.alive = false;
				continue;
			}
		}

		// Kart collision
		for (int k = 0; k < static_cast<int>(game.karts.size()); ++k)
		{
			if (k == proj.ownerKart && proj.lifetime > (proj.type == ItemType::GreenShell
				? GREEN_SHELL_LIFETIME - 0.3f : RED_SHELL_LIFETIME - 0.3f)) { continue; }
			KartState &target = game.karts[k];
			float dist = glm::length(target.position - proj.position);
			float hitRadius = (proj.type == ItemType::GreenShell ? GREEN_SHELL_RADIUS : RED_SHELL_RADIUS) + KART_HIT_RADIUS;
			if (dist < hitRadius)
			{
				target.spinoutTimer = BANANA_SPINOUT_TIME;
				target.speed *= BANANA_HIT_SPEED_MULT;
				if (target.driftState != DriftState::None)
				{
					target.driftState = DriftState::None;
					target.driftTimer = 0.f;
				}
				target.boostTimer = 0.f;
				proj.alive = false;
				game.events.push({GameEventType::KartHitProjectile, k, proj.ownerKart});
				break;
			}
		}
	}

	// Hazard collision
	for (Hazard &haz : game.hazards)
	{
		if (!haz.alive) { continue; }

		for (int k = 0; k < static_cast<int>(game.karts.size()); ++k)
		{
			KartState &kart = game.karts[k];
			if (kart.spinoutTimer > 0.f) { continue; }
			float dist = glm::length(kart.position - haz.position);
			if (dist < KART_HIT_RADIUS + 0.5f)
			{
				kart.spinoutTimer = BANANA_SPINOUT_TIME;
				kart.speed *= BANANA_HIT_SPEED_MULT;
				if (kart.driftState != DriftState::None)
				{
					kart.driftState = DriftState::None;
					kart.driftTimer = 0.f;
				}
				kart.boostTimer = 0.f;
				haz.alive = false;
				game.events.push({GameEventType::KartHitHazard, k, haz.ownerKart});
				break;
			}
		}
	}

	// Cleanup dead projectiles and hazards
	game.projectiles.erase(
		std::remove_if(game.projectiles.begin(), game.projectiles.end(),
			[](const Projectile &p) { return !p.alive; }),
		game.projectiles.end());
	game.hazards.erase(
		std::remove_if(game.hazards.begin(), game.hazards.end(),
			[](const Hazard &h) { return !h.alive; }),
		game.hazards.end());
}
