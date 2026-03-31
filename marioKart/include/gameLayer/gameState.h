#pragma once

#include "gameEvents.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <platform/platformInput.h>
#include <vector>

enum class RacePhase
{
	MainMenu,
	KartSelect,
	Boot,
	Countdown,
	Racing,
	Finished
};

enum class KartControlType
{
	Player,
	AI
};

enum class DriftState
{
	None,
	Hopping,
	Drifting
};

enum class ItemType
{
	None,
	Mushroom,
	Banana,
	GreenShell,
	RedShell
};

struct KartProgress
{
	int currentLap = 0;
	int checkpointIndex = 0;
	float segmentProgress = 0.f;
	bool finished = 0;
};

struct KartInputState
{
	float throttle = 0.f;
	float steer = 0.f;
	bool brake = 0;
	bool driftPressed = 0;
	bool useItemPressed = 0;
};

struct KartState
{
	KartControlType controlType = KartControlType::Player;

	glm::vec3 position = {};
	glm::vec3 velocity = {};
	glm::vec3 color = {0.2f, 0.6f, 1.f};

	float heading = 0.f;
	float speed = 0.f;
	float desiredSpeed = 0.f;
	float baseSpeed = 0.f;
	float laneOffset = 0.f;
	float distanceAlongTrack = 0.f;
	float boostTimer = 0.f;
	DriftState driftState = DriftState::None;
	int driftDirection = 0;
	float driftTimer = 0.f;
	float driftHopTimer = 0.f;
	bool offRoad = false;
	bool wrongWay = false;
	float wrongWayTimer = 0.f;

	ItemType heldItem = ItemType::None;
	float spinoutTimer = 0.f;
	float aiItemTimer = 0.f;

	KartInputState input = {};
	KartProgress progress = {};

	glm::vec3 lastSafePosition = {};
	float lastSafeHeading = 0.f;
	float stuckTimer = 0.f;
};

struct Checkpoint
{
	glm::vec3 start = {};
	glm::vec3 end = {};
	glm::vec3 forward = {};
	float distanceAlongTrack = 0.f;
};

struct BoostPad
{
	glm::vec3 position = {};
	glm::vec3 direction = {};
	float halfWidth = 2.f;
	float halfLength = 1.5f;
};

struct ItemBox
{
	glm::vec3 position = {};
	float respawnTimer = 0.f;
	bool active = true;
};

struct Projectile
{
	glm::vec3 position = {};
	glm::vec3 velocity = {};
	float heading = 0.f;
	ItemType type = ItemType::GreenShell;
	int ownerKart = -1;
	int targetKart = -1;
	float lifetime = 0.f;
	int bounceCount = 0;
	bool alive = false;
};

struct Hazard
{
	glm::vec3 position = {};
	int ownerKart = -1;
	bool alive = false;
};

struct TrackState
{
	std::vector<glm::vec3> centerLine;
	std::vector<Checkpoint> checkpoints;
	std::vector<BoostPad> boostPads;
	std::vector<ItemBox> itemBoxes;

	float totalLength = 0.f;
	float roadHalfWidth = 0.f;
	float wallHalfWidth = 0.f;

	glm::vec2 boundsMin = {};
	glm::vec2 boundsMax = {};
};

struct CameraState
{
	glm::vec3 position = {};
	glm::vec3 target = {};
	float distance = 12.f;
	float height = 8.f;
};

struct RaceState
{
	RacePhase phase = RacePhase::Boot;
	int totalLaps = 3;
	int playerKartIndex = 0;
	int playerPlace = 1;
	float countdownTimer = 3.f;
	float raceTimer = 0.f;
	float finishedTimer = 0.f;
	std::vector<int> ranking;
};

struct DebugState
{
	bool showOverlay = 1;
	float eventFlashTimer = 0.f;
};

struct MenuState
{
	int selectedKartSlot = 0;
	float previewRotation = 0.f;
};

struct GameState
{
	MenuState menu = {};
	RaceState race = {};
	TrackState track = {};
	CameraState camera = {};
	DebugState debug = {};

	std::vector<KartState> karts;
	std::vector<Projectile> projectiles;
	std::vector<Hazard> hazards;
	EventQueue events = {};

	float pulseTimer = 0.f;
};

GameState createDefaultGameState();
void resetRace(GameState &game);
void processGameInput(GameState &game, platform::Input &input);
void updateGameScaffold(GameState &game, float deltaTime);

const char *getRacePhaseName(RacePhase phase);
glm::vec3 getKartPaletteColor(int index);
