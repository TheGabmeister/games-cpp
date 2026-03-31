#pragma once

#include <vector>

enum class GameEventType
{
	RaceStarted,
	RaceFinished,
	CheckpointPassed,
	LapCompleted,
	KartHitWall,
	RespawnRequested,
	DriftBoosted,
	BoostPadHit
};

struct GameEvent
{
	GameEventType type = GameEventType::RaceStarted;
	int subjectKart = -1;
	int otherEntity = -1;
	int checkpointIndex = -1;
	int lapIndex = -1;
	float value = 0.f;
};

struct EventQueue
{
	std::vector<GameEvent> current;

	void clear();
	void push(const GameEvent &event);
};

const char *getGameEventName(GameEventType type);
