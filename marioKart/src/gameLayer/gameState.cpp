#include "gameState.h"
#include "gameConfig.h"
#include "trackSystems.h"
#include "itemSystems.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <logs.h>

namespace
{
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

glm::vec3 getKartPaletteColor(int index)
{
	static const glm::vec3 palette[KART_PALETTE_COUNT] =
	{
		{0.18f, 0.55f, 0.95f},
		{0.95f, 0.3f, 0.2f},
		{0.95f, 0.8f, 0.2f},
		{0.2f, 0.85f, 0.45f},
		{0.75f, 0.25f, 0.85f},
		{0.95f, 0.55f, 0.7f},
		{0.4f, 0.75f, 0.85f},
		{0.85f, 0.6f, 0.3f},
	};
	return palette[index % KART_PALETTE_COUNT];
}

GameState createDefaultGameState()
{
	GameState game = {};
	buildTrack(game.track);

	float laneOffsets[KART_PALETTE_COUNT] = {-2.5f, -0.8f, 0.9f, 2.6f, -1.8f, 1.6f, -3.2f, 3.4f};
	float baseSpeeds[KART_PALETTE_COUNT] = {12.f, 12.8f, 12.5f, 13.0f, 12.3f, 12.7f, 12.2f, 13.2f};

	game.karts.resize(KART_PALETTE_COUNT);
	for (int i = 0; i < KART_PALETTE_COUNT; ++i)
	{
		game.karts[i].controlType = (i == 0) ? KartControlType::Player : KartControlType::AI;
		game.karts[i].color = getKartPaletteColor(i);
		game.karts[i].laneOffset = laneOffsets[i];
		game.karts[i].baseSpeed = baseSpeeds[i];
		game.karts[i].distanceAlongTrack = i * 1.6f;
	}

	game.race.phase = RacePhase::MainMenu;
	game.menu = {};
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
		kart.spinoutTimer = 0.f;
		kart.aiItemTimer = 0.f;
		kart.stuckTimer = 0.f;
		kart.respawnTimer = 0.f;
		kart.progress = {};
		kart.distanceAlongTrack = i * 1.6f;

		updateKartTransform(kart, game.track);
		kart.progress.segmentProgress = getSegmentProgress(kart, game.track);
	}

	game.projectiles.clear();
	game.hazards.clear();
	for (ItemBox &box : game.track.itemBoxes) { box.active = true; box.respawnTimer = 0.f; }

	updateRaceRanking(game);
	platform::log("Phase 0.1 scaffold reset");
}

void processGameInput(GameState &game, platform::Input &input)
{
	if (game.race.phase == RacePhase::MainMenu)
	{
		if (input.isButtonPressed(platform::Button::Enter) ||
			input.isButtonPressed(platform::Button::Space))
		{
			game.race.phase = RacePhase::KartSelect;
			game.menu.selectedKartSlot = 0;
			game.menu.previewRotation = 0.f;
		}
		return;
	}

	if (game.race.phase == RacePhase::KartSelect)
	{
		if (input.isButtonPressed(platform::Button::Left))
		{
			game.menu.selectedKartSlot =
				(game.menu.selectedKartSlot - 1 + KART_PALETTE_COUNT) % KART_PALETTE_COUNT;
		}
		if (input.isButtonPressed(platform::Button::Right))
		{
			game.menu.selectedKartSlot =
				(game.menu.selectedKartSlot + 1) % KART_PALETTE_COUNT;
		}
		if (input.isButtonPressed(platform::Button::Enter) ||
			input.isButtonPressed(platform::Button::Space))
		{
			glm::vec3 chosenColor = getKartPaletteColor(game.menu.selectedKartSlot);
			glm::vec3 oldPlayerColor = game.karts[0].color;
			game.karts[0].color = chosenColor;

			for (int i = 1; i < static_cast<int>(game.karts.size()); ++i)
			{
				if (game.karts[i].color == chosenColor)
				{
					game.karts[i].color = oldPlayerColor;
					break;
				}
			}

			resetRace(game);
		}
		if (input.isButtonPressed(platform::Button::Escape))
		{
			game.race.phase = RacePhase::MainMenu;
		}
		return;
	}

	if (input.isButtonPressed(platform::Button::Tab))
	{
		game.debug.showOverlay = !game.debug.showOverlay;
	}

	if (game.race.phase == RacePhase::Finished)
	{
		if (input.isButtonPressed(platform::Button::Enter))
		{
			game.race.phase = RacePhase::MainMenu;
			game.menu = {};
		}
		return;
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
			kart.input.useItemPressed = input.isButtonPressed(platform::Button::Space);
		}
	}
}

void updateGameScaffold(GameState &game, float deltaTime)
{
	game.events.clear();
	game.pulseTimer += deltaTime;
	game.debug.eventFlashTimer = glm::max(0.f, game.debug.eventFlashTimer - deltaTime);

	if (game.race.phase == RacePhase::MainMenu)
	{
		return;
	}

	if (game.race.phase == RacePhase::KartSelect)
	{
		game.menu.previewRotation += MENU_KART_SPIN_SPEED * deltaTime;
		return;
	}

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

		// Spinout countdown (shared)
		if (kart.spinoutTimer > 0.f)
		{
			kart.spinoutTimer -= deltaTime;
			if (kart.spinoutTimer < 0.f) { kart.spinoutTimer = 0.f; }
			kart.heading += 8.f * deltaTime;
			kart.speed *= 0.95f;
		}

		if (kart.controlType == KartControlType::Player)
		{
			// Respawn freeze countdown
			if (kart.respawnTimer > 0.f)
			{
				kart.respawnTimer -= deltaTime;
				if (kart.respawnTimer < 0.f) { kart.respawnTimer = 0.f; }
			}

			bool playerFrozen = frozen || kart.respawnTimer > 0.f;

			float boostBonus = (kart.boostTimer > 0.f) ? BOOST_EXTRA_SPEED : 0.f;
			float accelFactor = kart.offRoad ? OFF_ROAD_ACCEL_FACTOR : 1.f;
			float maxSpeed = (kart.offRoad ? (KART_MAX_SPEED * OFF_ROAD_SPEED_FACTOR) : KART_MAX_SPEED) + boostBonus;
			float drag = KART_DRAG + (kart.offRoad ? OFF_ROAD_EXTRA_DRAG : 0.f);

			if (playerFrozen)
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

			// Save safe position only when on-road and moving forward
			if (!kart.offRoad && !kart.wrongWay && kart.speed > 1.f)
			{
				kart.lastSafePosition = kart.position;
				kart.lastSafeHeading = kart.heading;
				kart.stuckTimer = 0.f;
			}

			// Out-of-bounds / stuck detection
			if (game.race.phase == RacePhase::Racing && kart.respawnTimer <= 0.f)
			{
				bool outOfBounds = q.lateralDistance > game.track.wallHalfWidth + PLAYER_OOB_MARGIN;
				if (outOfBounds || std::abs(kart.speed) < PLAYER_STUCK_SPEED_THRESHOLD)
				{
					kart.stuckTimer += deltaTime;
				}

				if (outOfBounds || kart.stuckTimer >= PLAYER_STUCK_TIME)
				{
					kart.position = kart.lastSafePosition;
					kart.heading = kart.lastSafeHeading;
					kart.position.y = 0.f;
					kart.speed = 0.f;
					kart.velocity = {};
					kart.driftState = DriftState::None;
					kart.driftTimer = 0.f;
					kart.boostTimer = 0.f;
					kart.spinoutTimer = 0.f;
					kart.wrongWay = false;
					kart.wrongWayTimer = 0.f;
					kart.stuckTimer = 0.f;
					kart.respawnTimer = RESPAWN_FREEZE_TIME;

					TrackQuery rq = queryTrackPosition(kart.position, game.track);
					kart.distanceAlongTrack = rq.distanceAlongTrack;

					game.events.push({GameEventType::RespawnRequested, i});
				}
			}
		}
		else
		{
			// AI: waypoint-based movement with pacing

			// Corner speed: look ahead and slow for sharp turns
			float aheadDist = kart.distanceAlongTrack + AI_CORNER_LOOK_AHEAD;
			glm::vec3 currentFwd = sampleTrackForward(game.track, kart.distanceAlongTrack);
			glm::vec3 aheadFwd = sampleTrackForward(game.track, aheadDist);
			float cornerDot = glm::dot(currentFwd, aheadFwd);
			float cornerFactor = 1.f;
			if (cornerDot < (1.f - AI_CORNER_ANGLE_THRESHOLD))
			{
				cornerFactor = glm::mix(AI_CORNER_SPEED_FACTOR, 1.f,
					glm::clamp((cornerDot - 0.f) / (1.f - AI_CORNER_ANGLE_THRESHOLD), 0.f, 1.f));
			}

			// Rubber-banding: adjust speed based on distance to player
			const KartState &playerKart = game.karts[game.race.playerKartIndex];
			float playerDist = playerKart.distanceAlongTrack
				+ playerKart.progress.currentLap * game.track.totalLength;
			float aiDist = kart.distanceAlongTrack
				+ kart.progress.currentLap * game.track.totalLength;
			float gap = playerDist - aiDist;
			float rubberBand = 0.f;
			if (gap > 0.f)
			{
				rubberBand = AI_RUBBER_BAND_BOOST * glm::clamp(gap / AI_RUBBER_BAND_DISTANCE, 0.f, 1.f);
			}
			else
			{
				rubberBand = -AI_RUBBER_BAND_SLOW * glm::clamp(-gap / AI_RUBBER_BAND_DISTANCE, 0.f, 1.f);
			}

			float variation = std::sin(game.race.raceTimer * 0.7f + i * 0.8f) * 0.85f;
			kart.desiredSpeed = (kart.baseSpeed + variation + rubberBand) * cornerFactor;
			if (kart.boostTimer > 0.f) { kart.desiredSpeed += BOOST_EXTRA_SPEED; }

			if (frozen) { kart.desiredSpeed = 0.f; }

			float blendRate = (kart.desiredSpeed > kart.speed) ? 4.5f : 6.5f;
			kart.speed = glm::mix(kart.speed, kart.desiredSpeed, glm::clamp(deltaTime * blendRate, 0.f, 1.f));

			kart.distanceAlongTrack += kart.speed * deltaTime;

			// Oscillating lane offset for varied paths
			float baseLane = kart.laneOffset;
			float laneOscillation = std::sin(kart.distanceAlongTrack * 0.08f + i * 1.5f) * 1.5f;
			float effectiveLane = baseLane + laneOscillation;
			effectiveLane = glm::clamp(effectiveLane, -game.track.roadHalfWidth + 1.f,
				game.track.roadHalfWidth - 1.f);

			glm::vec3 trackForward = sampleTrackForward(game.track, kart.distanceAlongTrack);
			glm::vec3 right = glm::normalize(glm::vec3{-trackForward.z, 0.f, trackForward.x});
			kart.position = sampleTrackPosition(game.track, kart.distanceAlongTrack) + right * effectiveLane;
			kart.velocity = trackForward * kart.speed;
			kart.heading = std::atan2(trackForward.z, trackForward.x);

			// Stuck detection and recovery
			if (!frozen && game.race.phase == RacePhase::Racing)
			{
				if (std::abs(kart.speed) < AI_STUCK_SPEED_THRESHOLD)
				{
					kart.stuckTimer += deltaTime;
					if (kart.stuckTimer >= AI_STUCK_TIME)
					{
						kart.distanceAlongTrack += 10.f;
						kart.speed = kart.baseSpeed * 0.5f;
						kart.stuckTimer = 0.f;
						game.events.push({GameEventType::AIRecovered, i});
					}
				}
				else
				{
					kart.stuckTimer = 0.f;
				}
			}

			kart.lastSafePosition = kart.position;
			kart.lastSafeHeading = kart.heading;
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

	updateItemSystems(game, deltaTime);

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
		case RacePhase::MainMenu: return "MainMenu";
		case RacePhase::KartSelect: return "KartSelect";
		case RacePhase::Boot: return "Boot";
		case RacePhase::Countdown: return "Countdown";
		case RacePhase::Racing: return "Racing";
		case RacePhase::Finished: return "Finished";
		default: return "Unknown";
	}
}
