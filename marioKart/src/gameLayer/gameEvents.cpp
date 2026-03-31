#include "gameEvents.h"

void EventQueue::clear()
{
	current.clear();
}

void EventQueue::push(const GameEvent &event)
{
	current.push_back(event);
}

const char *getGameEventName(GameEventType type)
{
	switch (type)
	{
		case GameEventType::RaceStarted: return "RaceStarted";
		case GameEventType::RaceFinished: return "RaceFinished";
		case GameEventType::CheckpointPassed: return "CheckpointPassed";
		case GameEventType::LapCompleted: return "LapCompleted";
		case GameEventType::KartHitWall: return "KartHitWall";
		case GameEventType::RespawnRequested: return "RespawnRequested";
		case GameEventType::DriftBoosted: return "DriftBoosted";
		case GameEventType::BoostPadHit: return "BoostPadHit";
		case GameEventType::AIRecovered: return "AIRecovered";
		default: return "UnknownEvent";
	}
}
