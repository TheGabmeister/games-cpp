#include "gameState.h"

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

	void buildPlaceholderTrack(TrackState &track)
	{
		track.centerLine.clear();
		track.checkpoints.clear();

		track.centerLine.push_back({-28.f, 0.f, -18.f});
		track.centerLine.push_back({28.f, 0.f, -18.f});
		track.centerLine.push_back({28.f, 0.f, 18.f});
		track.centerLine.push_back({-28.f, 0.f, 18.f});

		track.roadHalfWidth = 6.f;
		track.boundsMin = {-40.f, -28.f};
		track.boundsMax = {40.f, 28.f};
		track.totalLength = 0.f;

		for (size_t i = 0; i < track.centerLine.size(); ++i)
		{
			size_t nextIndex = (i + 1) % track.centerLine.size();
			float segmentLength = glm::length(track.centerLine[nextIndex] - track.centerLine[i]);
			track.totalLength += segmentLength;
		}

		Checkpoint startLine = {};
		startLine.start = {-28.f, 0.f, -24.f};
		startLine.end = {-28.f, 0.f, -12.f};
		startLine.forward = {1.f, 0.f, 0.f};
		startLine.distanceAlongTrack = 0.f;
		track.checkpoints.push_back(startLine);

		Checkpoint checkpoint1 = {};
		checkpoint1.start = {22.f, 0.f, -18.f};
		checkpoint1.end = {34.f, 0.f, -18.f};
		checkpoint1.forward = {0.f, 0.f, 1.f};
		checkpoint1.distanceAlongTrack = 50.f;
		track.checkpoints.push_back(checkpoint1);

		Checkpoint checkpoint2 = {};
		checkpoint2.start = {28.f, 0.f, 12.f};
		checkpoint2.end = {28.f, 0.f, 24.f};
		checkpoint2.forward = {-1.f, 0.f, 0.f};
		checkpoint2.distanceAlongTrack = 92.f;
		track.checkpoints.push_back(checkpoint2);

		Checkpoint checkpoint3 = {};
		checkpoint3.start = {-22.f, 0.f, 18.f};
		checkpoint3.end = {-34.f, 0.f, 18.f};
		checkpoint3.forward = {0.f, 0.f, -1.f};
		checkpoint3.distanceAlongTrack = 142.f;
		track.checkpoints.push_back(checkpoint3);
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
	buildPlaceholderTrack(game.track);

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

		if (kart.controlType == KartControlType::Player)
		{
			float targetSpeed = kart.baseSpeed;
			if (kart.input.throttle > 0.f)
			{
				targetSpeed += 3.5f;
			}
			if (kart.input.brake)
			{
				targetSpeed -= 5.f;
			}

			kart.desiredSpeed = glm::clamp(targetSpeed, 2.f, 18.f);
		}
		else
		{
			float variation = std::sin(game.race.raceTimer * 0.7f + i * 0.8f) * 0.85f;
			kart.desiredSpeed = kart.baseSpeed + variation;
		}

		if (game.race.phase == RacePhase::Countdown || game.race.phase == RacePhase::Finished)
		{
			kart.desiredSpeed = 0.f;
		}

		float blendRate = (kart.desiredSpeed > kart.speed) ? 4.5f : 6.5f;
		kart.speed = glm::mix(kart.speed, kart.desiredSpeed, glm::clamp(deltaTime * blendRate, 0.f, 1.f));

		float previousDistance = kart.distanceAlongTrack;
		kart.distanceAlongTrack += kart.speed * deltaTime;

		if (game.race.phase == RacePhase::Racing && !kart.progress.finished)
		{
			for (int nextCheckpoint = kart.progress.checkpointIndex + 1;
				nextCheckpoint < static_cast<int>(game.track.checkpoints.size());
				++nextCheckpoint)
			{
				const Checkpoint &checkpoint = game.track.checkpoints[nextCheckpoint];
				if (!crossedWrappedDistance(previousDistance, kart.distanceAlongTrack,
					checkpoint.distanceAlongTrack, game.track.totalLength))
				{
					break;
				}

				kart.progress.checkpointIndex = nextCheckpoint;
				game.events.push({GameEventType::CheckpointPassed, i, -1, nextCheckpoint});
			}

			bool crossedStart = crossedWrappedDistance(previousDistance, kart.distanceAlongTrack, 0.f, game.track.totalLength);
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

		updateKartTransform(kart, game.track);
		kart.progress.segmentProgress = getSegmentProgress(kart, game.track);
	}

	glm::vec3 playerPosition = game.karts[game.race.playerKartIndex].position;
	glm::vec3 playerForward = sampleTrackForward(game.track, game.karts[game.race.playerKartIndex].distanceAlongTrack);
	game.camera.target = playerPosition;
	game.camera.position = playerPosition - playerForward * game.camera.distance + glm::vec3(0.f, game.camera.height, 0.f);

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
