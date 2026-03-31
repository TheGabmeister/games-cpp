#include "gameState.h"
#include "gameConfig.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <logs.h>

namespace
{
	float wrapDistance(float value, float length)
	{
		if (length <= 0.f) { return 0.f; }

		float wrapped = std::fmod(value, length);
		if (wrapped < 0.f)
		{
			wrapped += length;
		}
		return wrapped;
	}

	bool crossedWrappedDistance(float previousValue, float currentValue, float threshold, float length)
	{
		float previousWrapped = wrapDistance(previousValue, length);
		float currentWrapped = wrapDistance(currentValue, length);
		float wrappedThreshold = wrapDistance(threshold, length);

		if (currentValue - previousValue >= length)
		{
			return 1;
		}

		if (currentWrapped >= previousWrapped)
		{
			return previousWrapped < wrappedThreshold && currentWrapped >= wrappedThreshold;
		}

		return previousWrapped < wrappedThreshold || currentWrapped >= wrappedThreshold;
	}

	glm::vec3 sampleTrackPosition(const TrackState &track, float distanceAlongTrack)
	{
		if (track.centerLine.size() < 2 || track.totalLength <= 0.f)
		{
			return {};
		}

		float remainingDistance = wrapDistance(distanceAlongTrack, track.totalLength);

		for (size_t i = 0; i < track.centerLine.size(); ++i)
		{
			size_t nextIndex = (i + 1) % track.centerLine.size();
			glm::vec3 start = track.centerLine[i];
			glm::vec3 end = track.centerLine[nextIndex];
			float segmentLength = glm::length(end - start);

			if (segmentLength <= 0.f)
			{
				continue;
			}

			if (remainingDistance <= segmentLength)
			{
				float factor = remainingDistance / segmentLength;
				return glm::mix(start, end, factor);
			}

			remainingDistance -= segmentLength;
		}

		return track.centerLine.front();
	}

	glm::vec3 sampleTrackForward(const TrackState &track, float distanceAlongTrack)
	{
		if (track.centerLine.size() < 2 || track.totalLength <= 0.f)
		{
			return {1.f, 0.f, 0.f};
		}

		float remainingDistance = wrapDistance(distanceAlongTrack, track.totalLength);

		for (size_t i = 0; i < track.centerLine.size(); ++i)
		{
			size_t nextIndex = (i + 1) % track.centerLine.size();
			glm::vec3 start = track.centerLine[i];
			glm::vec3 end = track.centerLine[nextIndex];
			glm::vec3 delta = end - start;
			float segmentLength = glm::length(delta);

			if (segmentLength <= 0.f)
			{
				continue;
			}

			if (remainingDistance <= segmentLength)
			{
				return delta / segmentLength;
			}

			remainingDistance -= segmentLength;
		}

		glm::vec3 fallback = track.centerLine[1] - track.centerLine[0];
		float fallbackLength = glm::length(fallback);
		if (fallbackLength <= 0.f)
		{
			return {1.f, 0.f, 0.f};
		}

		return fallback / fallbackLength;
	}

	void buildTrack(TrackState &track)
	{
		track.centerLine.clear();
		track.checkpoints.clear();

		// Oval track (stadium shape): clockwise loop when viewed from above
		// Bottom straight (heading +X)
		track.centerLine.push_back({-24.f, 0.f, -16.f});  // 0: start/finish
		track.centerLine.push_back({-10.f, 0.f, -16.f});  // 1
		track.centerLine.push_back({10.f, 0.f, -16.f});   // 2

		// Right turn
		track.centerLine.push_back({20.f, 0.f, -14.f});   // 3
		track.centerLine.push_back({26.f, 0.f, -8.f});    // 4
		track.centerLine.push_back({28.f, 0.f, 0.f});     // 5: right apex
		track.centerLine.push_back({26.f, 0.f, 8.f});     // 6
		track.centerLine.push_back({20.f, 0.f, 14.f});    // 7

		// Top straight (heading -X)
		track.centerLine.push_back({10.f, 0.f, 16.f});    // 8
		track.centerLine.push_back({-10.f, 0.f, 16.f});   // 9

		// Left turn
		track.centerLine.push_back({-20.f, 0.f, 14.f});   // 10
		track.centerLine.push_back({-26.f, 0.f, 8.f});    // 11
		track.centerLine.push_back({-28.f, 0.f, 0.f});    // 12: left apex
		track.centerLine.push_back({-26.f, 0.f, -8.f});   // 13
		track.centerLine.push_back({-20.f, 0.f, -14.f});  // 14

		track.roadHalfWidth = 6.f;
		track.wallHalfWidth = 10.f;

		// Compute total length
		track.totalLength = 0.f;
		for (size_t i = 0; i < track.centerLine.size(); ++i)
		{
			size_t next = (i + 1) % track.centerLine.size();
			track.totalLength += glm::length(track.centerLine[next] - track.centerLine[i]);
		}

		// Compute bounds from centerline + wall margin
		track.boundsMin = {FLT_MAX, FLT_MAX};
		track.boundsMax = {-FLT_MAX, -FLT_MAX};
		for (const auto &p : track.centerLine)
		{
			track.boundsMin = glm::min(track.boundsMin, glm::vec2(p.x, p.z));
			track.boundsMax = glm::max(track.boundsMax, glm::vec2(p.x, p.z));
		}
		track.boundsMin -= glm::vec2(track.wallHalfWidth + 5.f);
		track.boundsMax += glm::vec2(track.wallHalfWidth + 5.f);

		// Place checkpoints at specific centerline point indices
		auto addCheckpoint = [&](int pointIndex, glm::vec3 forward)
		{
			Checkpoint cp = {};
			glm::vec3 pos = track.centerLine[pointIndex];
			glm::vec3 right = glm::vec3(-forward.z, 0.f, forward.x);
			cp.start = pos - right * (track.roadHalfWidth + 4.f);
			cp.end = pos + right * (track.roadHalfWidth + 4.f);
			cp.forward = forward;

			float dist = 0.f;
			for (int i = 0; i < pointIndex; ++i)
			{
				dist += glm::length(track.centerLine[i + 1] - track.centerLine[i]);
			}
			cp.distanceAlongTrack = dist;

			track.checkpoints.push_back(cp);
		};

		addCheckpoint(0, {1.f, 0.f, 0.f});    // Start/finish
		addCheckpoint(5, {0.f, 0.f, 1.f});     // Right apex
		addCheckpoint(9, {-1.f, 0.f, 0.f});    // Top-left
		addCheckpoint(12, {0.f, 0.f, -1.f});   // Left apex

		// Boost pads
		track.boostPads.clear();
		track.boostPads.push_back({{0.f, 0.01f, -16.f}, {1.f, 0.f, 0.f}, 3.f, 1.5f});
		track.boostPads.push_back({{0.f, 0.01f, 16.f}, {-1.f, 0.f, 0.f}, 3.f, 1.5f});
	}

	void updateKartTransform(KartState &kart, const TrackState &track)
	{
		glm::vec3 trackForward = sampleTrackForward(track, kart.distanceAlongTrack);
		glm::vec3 right = glm::normalize(glm::vec3{-trackForward.z, 0.f, trackForward.x});
		kart.position = sampleTrackPosition(track, kart.distanceAlongTrack) + right * kart.laneOffset;
		kart.velocity = trackForward * kart.speed;
		kart.heading = std::atan2(trackForward.z, trackForward.x);
		kart.lastSafePosition = kart.position;
		kart.lastSafeHeading = kart.heading;
	}

	float getSegmentProgress(const KartState &kart, const TrackState &track)
	{
		if (track.checkpoints.empty() || track.totalLength <= 0.f)
		{
			return 0.f;
		}

		float localDistance = wrapDistance(kart.distanceAlongTrack, track.totalLength);
		float previousDistance = track.checkpoints[kart.progress.checkpointIndex].distanceAlongTrack;
		float nextDistance = track.totalLength;

		if (kart.progress.checkpointIndex + 1 < static_cast<int>(track.checkpoints.size()))
		{
			nextDistance = track.checkpoints[kart.progress.checkpointIndex + 1].distanceAlongTrack;
		}

		if (nextDistance < previousDistance)
		{
			nextDistance += track.totalLength;
		}

		if (localDistance < previousDistance)
		{
			localDistance += track.totalLength;
		}

		float segmentLength = nextDistance - previousDistance;
		if (segmentLength <= 0.f)
		{
			return 0.f;
		}

		return glm::clamp((localDistance - previousDistance) / segmentLength, 0.f, 1.f);
	}

	struct TrackQuery
	{
		float distanceAlongTrack = 0.f;
		float lateralDistance = 0.f;
		glm::vec3 closestPoint = {};
		glm::vec3 normal = {};
	};

	TrackQuery queryTrackPosition(glm::vec3 position, const TrackState &track)
	{
		TrackQuery result = {};
		float bestDistSq = FLT_MAX;
		float runningDist = 0.f;

		for (size_t i = 0; i < track.centerLine.size(); ++i)
		{
			size_t next = (i + 1) % track.centerLine.size();
			glm::vec3 a = track.centerLine[i];
			glm::vec3 b = track.centerLine[next];
			glm::vec3 ab = b - a;
			float segLen = glm::length(ab);
			if (segLen <= 0.f) { runningDist += segLen; continue; }

			float t = glm::clamp(glm::dot(position - a, ab) / (segLen * segLen), 0.f, 1.f);
			glm::vec3 closest = a + ab * t;
			float distSq = glm::dot(position - closest, position - closest);

			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				result.distanceAlongTrack = runningDist + t * segLen;
				result.closestPoint = closest;
			}

			runningDist += segLen;
		}

		glm::vec3 offset = position - result.closestPoint;
		offset.y = 0.f;
		result.lateralDistance = glm::length(offset);
		result.normal = (result.lateralDistance > 0.001f)
			? offset / result.lateralDistance
			: glm::vec3(0.f, 0.f, 1.f);

		return result;
	}

	void updateRaceRanking(GameState &game)
	{
		game.race.ranking.clear();
		game.race.ranking.reserve(game.karts.size());

		for (int i = 0; i < static_cast<int>(game.karts.size()); ++i)
		{
			game.race.ranking.push_back(i);
		}

		std::sort(game.race.ranking.begin(), game.race.ranking.end(),
			[&](int leftIndex, int rightIndex)
			{
				const KartState &left = game.karts[leftIndex];
				const KartState &right = game.karts[rightIndex];

				if (left.progress.finished != right.progress.finished)
				{
					return left.progress.finished > right.progress.finished;
				}

				if (left.progress.currentLap != right.progress.currentLap)
				{
					return left.progress.currentLap > right.progress.currentLap;
				}

				if (left.progress.checkpointIndex != right.progress.checkpointIndex)
				{
					return left.progress.checkpointIndex > right.progress.checkpointIndex;
				}

				return left.progress.segmentProgress > right.progress.segmentProgress;
			});

		game.race.playerPlace = 1;
		for (int i = 0; i < static_cast<int>(game.race.ranking.size()); ++i)
		{
			if (game.race.ranking[i] == game.race.playerKartIndex)
			{
				game.race.playerPlace = i + 1;
				break;
			}
		}
	}

	void emitAndLogEvents(GameState &game)
	{
		if (game.events.current.empty())
		{
			return;
		}

		game.debug.eventFlashTimer = 0.35f;

		char buffer[160] = {};
		for (const GameEvent &event: game.events.current)
		{
			if (event.type == GameEventType::CheckpointPassed)
			{
				std::snprintf(buffer, sizeof(buffer), "%s: kart %d checkpoint %d",
					getGameEventName(event.type), event.subjectKart, event.checkpointIndex);
			}
			else if (event.type == GameEventType::LapCompleted)
			{
				std::snprintf(buffer, sizeof(buffer), "%s: kart %d lap %d",
					getGameEventName(event.type), event.subjectKart, event.lapIndex + 1);
			}
			else
			{
				std::snprintf(buffer, sizeof(buffer), "%s: kart %d",
					getGameEventName(event.type), event.subjectKart);
			}

			platform::log(buffer);
		}
	}
}

GameState createDefaultGameState()
{
	GameState game = {};
	buildTrack(game.track);

	game.karts.resize(4);

	game.karts[0].controlType = KartControlType::Player;
	game.karts[0].color = {0.18f, 0.55f, 0.95f};
	game.karts[0].laneOffset = -2.5f;
	game.karts[0].baseSpeed = 12.f;
	game.karts[0].distanceAlongTrack = 0.f;

	game.karts[1].controlType = KartControlType::AI;
	game.karts[1].color = {0.95f, 0.3f, 0.2f};
	game.karts[1].laneOffset = -0.8f;
	game.karts[1].baseSpeed = 13.2f;
	game.karts[1].distanceAlongTrack = 1.6f;

	game.karts[2].controlType = KartControlType::AI;
	game.karts[2].color = {0.95f, 0.8f, 0.2f};
	game.karts[2].laneOffset = 0.9f;
	game.karts[2].baseSpeed = 12.8f;
	game.karts[2].distanceAlongTrack = 3.2f;

	game.karts[3].controlType = KartControlType::AI;
	game.karts[3].color = {0.2f, 0.85f, 0.45f};
	game.karts[3].laneOffset = 2.6f;
	game.karts[3].baseSpeed = 13.5f;
	game.karts[3].distanceAlongTrack = 4.8f;

	resetRace(game);
	return game;
}

void resetRace(GameState &game)
{
	game.events.clear();
	game.pulseTimer = 0.f;
	game.debug.eventFlashTimer = 0.f;
	game.race.phase = RacePhase::Countdown;
	game.race.countdownTimer = 3.f;
	game.race.raceTimer = 0.f;
	game.race.finishedTimer = 0.f;
	game.race.playerPlace = 1;
	game.race.playerKartIndex = 0;
	game.race.totalLaps = 3;
	game.race.ranking.clear();

	for (int i = 0; i < static_cast<int>(game.karts.size()); ++i)
	{
		KartState &kart = game.karts[i];
		kart.speed = 0.f;
		kart.desiredSpeed = kart.baseSpeed;
		kart.boostTimer = 0.f;
		kart.driftState = DriftState::None;
		kart.driftDirection = 0;
		kart.driftTimer = 0.f;
		kart.driftHopTimer = 0.f;
		kart.offRoad = false;
		kart.wrongWay = false;
		kart.wrongWayTimer = 0.f;
		kart.heldItem = ItemType::None;
		kart.progress = {};
		kart.distanceAlongTrack = i * 1.6f;

		updateKartTransform(kart, game.track);
		kart.progress.segmentProgress = getSegmentProgress(kart, game.track);
	}

	updateRaceRanking(game);
	platform::log("Phase 0.1 scaffold reset");
}

void processGameInput(GameState &game, platform::Input &input)
{
	if (input.isButtonPressed(platform::Button::Tab))
	{
		game.debug.showOverlay = !game.debug.showOverlay;
	}

	if (input.isButtonPressed(platform::Button::Enter))
	{
		resetRace(game);
	}

	for (KartState &kart : game.karts)
	{
		if (kart.controlType == KartControlType::Player)
		{
			kart.input.throttle = input.isButtonHeld(platform::Button::Up) ? 1.f : 0.f;
			kart.input.brake = input.isButtonHeld(platform::Button::Down);
			kart.input.steer = 0.f;
			if (input.isButtonHeld(platform::Button::Left)) { kart.input.steer -= 1.f; }
			if (input.isButtonHeld(platform::Button::Right)) { kart.input.steer += 1.f; }
			kart.input.driftPressed = input.isButtonHeld(platform::Button::LeftShift);
		}
	}
}

void updateGameScaffold(GameState &game, float deltaTime)
{
	game.events.clear();
	game.pulseTimer += deltaTime;
	game.debug.eventFlashTimer = glm::max(0.f, game.debug.eventFlashTimer - deltaTime);

	if (game.race.phase == RacePhase::Countdown)
	{
		game.race.countdownTimer -= deltaTime;
		if (game.race.countdownTimer <= 0.f)
		{
			game.race.countdownTimer = 0.f;
			game.race.phase = RacePhase::Racing;
			game.events.push({GameEventType::RaceStarted, game.race.playerKartIndex});
		}
	}
	else if (game.race.phase == RacePhase::Racing)
	{
		game.race.raceTimer += deltaTime;
	}
	else if (game.race.phase == RacePhase::Finished)
	{
		game.race.finishedTimer += deltaTime;
	}

	for (int i = 0; i < static_cast<int>(game.karts.size()); ++i)
	{
		KartState &kart = game.karts[i];
		bool frozen = (game.race.phase == RacePhase::Countdown || game.race.phase == RacePhase::Finished);
		float previousDistance = kart.distanceAlongTrack;

		if (kart.controlType == KartControlType::Player)
		{
			float boostBonus = (kart.boostTimer > 0.f) ? BOOST_EXTRA_SPEED : 0.f;
			float accelFactor = kart.offRoad ? OFF_ROAD_ACCEL_FACTOR : 1.f;
			float maxSpeed = (kart.offRoad ? (KART_MAX_SPEED * OFF_ROAD_SPEED_FACTOR) : KART_MAX_SPEED) + boostBonus;
			float drag = KART_DRAG + (kart.offRoad ? OFF_ROAD_EXTRA_DRAG : 0.f);

			if (frozen)
			{
				if (kart.driftState != DriftState::None) { kart.driftState = DriftState::None; kart.driftTimer = 0.f; }
				if (kart.speed > 0.f) { kart.speed -= drag * deltaTime; if (kart.speed < 0.f) { kart.speed = 0.f; } }
				else if (kart.speed < 0.f) { kart.speed += drag * deltaTime; if (kart.speed > 0.f) { kart.speed = 0.f; } }
			}
			else if (kart.input.throttle > 0.f)
			{
				kart.speed += KART_ACCELERATION * accelFactor * deltaTime;
				if (kart.speed > maxSpeed) { kart.speed = maxSpeed; }
			}
			else if (kart.input.brake)
			{
				if (kart.speed > 0.f)
				{
					kart.speed -= KART_BRAKE_DECEL * deltaTime;
					if (kart.speed < 0.f) { kart.speed = 0.f; }
				}
				else
				{
					kart.speed -= KART_REVERSE_ACCEL * deltaTime;
					if (kart.speed < -KART_REVERSE_MAX_SPEED) { kart.speed = -KART_REVERSE_MAX_SPEED; }
				}
			}
			else
			{
				if (kart.speed > 0.f)
				{
					kart.speed -= drag * deltaTime;
					if (kart.speed < 0.f) { kart.speed = 0.f; }
				}
				else if (kart.speed < 0.f)
				{
					kart.speed += drag * deltaTime;
					if (kart.speed > 0.f) { kart.speed = 0.f; }
				}
			}

			// Off-road speed cap: drag down to reduced max if over it
			if (kart.offRoad && kart.speed > maxSpeed)
			{
				kart.speed -= OFF_ROAD_EXTRA_DRAG * deltaTime;
				if (kart.speed < maxSpeed) { kart.speed = maxSpeed; }
			}

			// Drift state machine
			if (kart.driftState == DriftState::None)
			{
				if (kart.input.driftPressed && std::abs(kart.input.steer) > 0.1f
					&& std::abs(kart.speed) > DRIFT_MIN_SPEED)
				{
					kart.driftState = DriftState::Hopping;
					kart.driftDirection = (kart.input.steer > 0.f) ? 1 : -1;
					kart.driftHopTimer = 0.f;
					kart.driftTimer = 0.f;
				}
			}
			else if (kart.driftState == DriftState::Hopping)
			{
				kart.driftHopTimer += deltaTime;
				if (kart.driftHopTimer >= DRIFT_HOP_DURATION)
				{
					kart.driftState = DriftState::Drifting;
				}
				if (!kart.input.driftPressed)
				{
					kart.driftState = DriftState::None;
				}
			}
			else if (kart.driftState == DriftState::Drifting)
			{
				kart.driftTimer += deltaTime;
				if (!kart.input.driftPressed || std::abs(kart.speed) < 1.f)
				{
					if (kart.driftTimer >= MINI_TURBO_MIN_DRIFT_TIME)
					{
						kart.boostTimer = MINI_TURBO_DURATION;
						game.events.push({GameEventType::DriftBoosted, i});
					}
					kart.driftState = DriftState::None;
					kart.driftTimer = 0.f;
				}
			}

			// Steering (only when moving)
			if (std::abs(kart.speed) > 0.1f)
			{
				float speedFactor = 1.f - KART_STEER_SPEED_REDUCTION *
					glm::clamp(std::abs(kart.speed) / KART_MAX_SPEED, 0.f, 1.f);
				if (kart.driftState == DriftState::Drifting)
				{
					float driftSteer = static_cast<float>(kart.driftDirection) * DRIFT_STEER_BIAS
						+ kart.input.steer * DRIFT_STEER_RATE;
					kart.heading -= driftSteer * speedFactor * deltaTime;
				}
				else
				{
					kart.heading -= kart.input.steer * KART_STEER_RATE * speedFactor * deltaTime;
				}
			}

			// Move by velocity
			glm::vec3 forward = {std::cos(kart.heading), 0.f, std::sin(kart.heading)};
			kart.velocity = forward * kart.speed;
			kart.position += kart.velocity * deltaTime;

			// Lateral slip during drift
			if (kart.driftState == DriftState::Drifting)
			{
				glm::vec3 right = {-std::sin(kart.heading), 0.f, std::cos(kart.heading)};
				float slipScale = glm::clamp(std::abs(kart.speed) / KART_MAX_SPEED, 0.f, 1.f);
				kart.position += right * (-static_cast<float>(kart.driftDirection)
					* DRIFT_LATERAL_SLIP * slipScale * deltaTime);
			}
			kart.position.y = 0.f;

			// Track query: wall collision, off-road, and distance
			TrackQuery q = queryTrackPosition(kart.position, game.track);
			kart.distanceAlongTrack = q.distanceAlongTrack;

			if (q.lateralDistance > game.track.wallHalfWidth)
			{
				kart.position = q.closestPoint + q.normal * game.track.wallHalfWidth;
				kart.position.y = 0.f;
				kart.speed *= WALL_BOUNCE_SPEED_RETAIN;
				if (kart.driftState != DriftState::None)
				{
					kart.driftState = DriftState::None;
					kart.driftTimer = 0.f;
				}
				kart.boostTimer *= 0.5f;
				game.events.push({GameEventType::KartHitWall, i});
			}

			kart.offRoad = q.lateralDistance > game.track.roadHalfWidth;

			// Wrong-way detection
			glm::vec3 trackFwd = sampleTrackForward(game.track, kart.distanceAlongTrack);
			glm::vec3 kartFwd = {std::cos(kart.heading), 0.f, std::sin(kart.heading)};
			if (glm::dot(kartFwd, trackFwd) < -0.3f && std::abs(kart.speed) > 1.f)
			{
				kart.wrongWayTimer += deltaTime;
				kart.wrongWay = kart.wrongWayTimer > WRONG_WAY_TIME;
			}
			else
			{
				kart.wrongWayTimer = 0.f;
				kart.wrongWay = false;
			}

			kart.lastSafePosition = kart.position;
			kart.lastSafeHeading = kart.heading;
		}
		else
		{
			// AI: on-rails movement
			float variation = std::sin(game.race.raceTimer * 0.7f + i * 0.8f) * 0.85f;
			kart.desiredSpeed = kart.baseSpeed + variation;
			if (kart.boostTimer > 0.f) { kart.desiredSpeed += BOOST_EXTRA_SPEED; }

			if (frozen) { kart.desiredSpeed = 0.f; }

			float blendRate = (kart.desiredSpeed > kart.speed) ? 4.5f : 6.5f;
			kart.speed = glm::mix(kart.speed, kart.desiredSpeed, glm::clamp(deltaTime * blendRate, 0.f, 1.f));

			kart.distanceAlongTrack += kart.speed * deltaTime;

			updateKartTransform(kart, game.track);
		}

		// Boost pad detection (shared)
		for (const BoostPad &pad : game.track.boostPads)
		{
			glm::vec3 toKart = kart.position - pad.position;
			glm::vec3 padForward = glm::normalize(pad.direction);
			glm::vec3 padRight = {-padForward.z, 0.f, padForward.x};
			float forwardDist = std::abs(glm::dot(toKart, padForward));
			float rightDist = std::abs(glm::dot(toKart, padRight));

			if (forwardDist < pad.halfLength && rightDist < pad.halfWidth)
			{
				if (kart.boostTimer <= 0.f)
				{
					game.events.push({GameEventType::BoostPadHit, i});
				}
				kart.boostTimer = BOOST_PAD_DURATION;
			}
		}

		// Boost timer countdown (shared)
		if (kart.boostTimer > 0.f)
		{
			kart.boostTimer -= deltaTime;
			if (kart.boostTimer < 0.f) { kart.boostTimer = 0.f; }
		}

		// Checkpoint and lap logic (shared)
		if (game.race.phase == RacePhase::Racing && !kart.progress.finished)
		{
			for (int nextCheckpoint = kart.progress.checkpointIndex + 1;
				nextCheckpoint < static_cast<int>(game.track.checkpoints.size());
				++nextCheckpoint)
			{
				const Checkpoint &checkpoint = game.track.checkpoints[nextCheckpoint];
				// For free-driving player, use position-based checkpoint crossing
				if (kart.controlType == KartControlType::Player)
				{
					glm::vec3 cpCenter = (checkpoint.start + checkpoint.end) * 0.5f;
					float cpRadius = glm::length(checkpoint.end - checkpoint.start) * 0.5f + 2.f;
					float dist = glm::length(kart.position - cpCenter);
					if (dist > cpRadius) { break; }
				}
				else
				{
					if (!crossedWrappedDistance(previousDistance, kart.distanceAlongTrack,
						checkpoint.distanceAlongTrack, game.track.totalLength))
					{
						break;
					}
				}

				kart.progress.checkpointIndex = nextCheckpoint;
				game.events.push({GameEventType::CheckpointPassed, i, -1, nextCheckpoint});
			}

			// Lap completion: player uses position near start line, AI uses distance
			bool crossedStart = false;
			if (kart.controlType == KartControlType::Player && !game.track.checkpoints.empty())
			{
				glm::vec3 startCenter = (game.track.checkpoints[0].start + game.track.checkpoints[0].end) * 0.5f;
				float startRadius = glm::length(game.track.checkpoints[0].end - game.track.checkpoints[0].start) * 0.5f + 2.f;
				crossedStart = glm::length(kart.position - startCenter) < startRadius;
			}
			else
			{
				crossedStart = crossedWrappedDistance(previousDistance, kart.distanceAlongTrack, 0.f, game.track.totalLength);
			}

			int finalCheckpointIndex = static_cast<int>(game.track.checkpoints.size()) - 1;
			if (crossedStart && kart.progress.checkpointIndex == finalCheckpointIndex)
			{
				kart.progress.currentLap += 1;
				kart.progress.checkpointIndex = 0;
				game.events.push({GameEventType::LapCompleted, i, -1, 0, kart.progress.currentLap - 1});

				if (kart.progress.currentLap >= game.race.totalLaps)
				{
					kart.progress.finished = 1;
				}

				if (i == game.race.playerKartIndex && kart.progress.finished)
				{
					game.race.phase = RacePhase::Finished;
					game.events.push({GameEventType::RaceFinished, i});
				}
			}
		}

		kart.progress.segmentProgress = getSegmentProgress(kart, game.track);
	}

	// Chase camera with spring smoothing
	const KartState &playerKart = game.karts[game.race.playerKartIndex];
	glm::vec3 kartForward = {std::cos(playerKart.heading), 0.f, std::sin(playerKart.heading)};
	glm::vec3 desiredTarget = playerKart.position + kartForward * CAMERA_LOOK_AHEAD;
	glm::vec3 desiredPosition = playerKart.position - kartForward * game.camera.distance
		+ glm::vec3(0.f, game.camera.height, 0.f);

	float posFactor = glm::clamp(CAMERA_POSITION_SMOOTH * deltaTime, 0.f, 1.f);
	float tgtFactor = glm::clamp(CAMERA_TARGET_SMOOTH * deltaTime, 0.f, 1.f);
	game.camera.position = glm::mix(game.camera.position, desiredPosition, posFactor);
	game.camera.target = glm::mix(game.camera.target, desiredTarget, tgtFactor);

	updateRaceRanking(game);
	emitAndLogEvents(game);
}

const char *getRacePhaseName(RacePhase phase)
{
	switch (phase)
	{
		case RacePhase::Boot: return "Boot";
		case RacePhase::Countdown: return "Countdown";
		case RacePhase::Racing: return "Racing";
		case RacePhase::Finished: return "Finished";
		default: return "Unknown";
	}
}
