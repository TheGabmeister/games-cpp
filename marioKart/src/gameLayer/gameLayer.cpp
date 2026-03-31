#include "gameLayer.h"
#include "gameState.h"
#include "gameConfig.h"
#include "renderer.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <logs.h>

namespace
{
	GameState gameData = {};
	float simulationAccumulator = 0.f;

	struct ScreenRect
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
	};

	ScreenRect clampRect(ScreenRect rect, int maxWidth, int maxHeight)
	{
		int right = rect.x + rect.width;
		int bottom = rect.y + rect.height;

		rect.x = std::max(0, rect.x);
		rect.y = std::max(0, rect.y);
		right = std::min(maxWidth, right);
		bottom = std::min(maxHeight, bottom);

		rect.width = std::max(0, right - rect.x);
		rect.height = std::max(0, bottom - rect.y);
		return rect;
	}

	void drawRectPixels(int framebufferHeight, int framebufferWidth, ScreenRect rect, glm::vec3 color)
	{
		rect = clampRect(rect, framebufferWidth, framebufferHeight);
		if (rect.width <= 0 || rect.height <= 0)
		{
			return;
		}

		glScissor(rect.x, framebufferHeight - rect.y - rect.height, rect.width, rect.height);
		glClearColor(color.r, color.g, color.b, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void drawWorld3D(const GameState &game)
	{
		const TrackState &track = game.track;

		// Ground plane (off-road color)
		glm::vec2 boundsSize = track.boundsMax - track.boundsMin;
		glm::vec2 boundsCenter = (track.boundsMin + track.boundsMax) * 0.5f;
		renderer::drawQuad({boundsCenter.x, -0.01f, boundsCenter.y},
			{boundsSize.x + 20.f, boundsSize.y + 20.f}, {0.22f, 0.3f, 0.2f});

		// Road segments
		float roadWidth = track.roadHalfWidth * 2.f;
		for (size_t i = 0; i < track.centerLine.size(); ++i)
		{
			size_t next = (i + 1) % track.centerLine.size();
			glm::vec3 start = track.centerLine[i];
			glm::vec3 end = track.centerLine[next];
			glm::vec3 delta = end - start;
			float segmentLength = glm::length(delta);
			if (segmentLength <= 0.f) { continue; }

			glm::vec3 center = (start + end) * 0.5f;
			center.y = 0.f;
			float angle = std::atan2(delta.x, delta.z);

			renderer::drawQuad(center,
				{roadWidth, segmentLength + roadWidth},
				{0.25f, 0.27f, 0.32f}, angle);
		}

		// Checkpoints
		for (int i = 0; i < static_cast<int>(track.checkpoints.size()); ++i)
		{
			const Checkpoint &cp = track.checkpoints[i];
			glm::vec3 center = (cp.start + cp.end) * 0.5f;
			center.y = 0.5f;
			glm::vec3 color = (i == 0)
				? glm::vec3{0.92f, 0.84f, 0.22f}
				: glm::vec3{0.84f, 0.48f, 0.2f};
			renderer::drawMarker(center, 3.f, color);
		}

		// Karts
		for (int i = 0; i < static_cast<int>(game.karts.size()); ++i)
		{
			const KartState &kart = game.karts[i];
			glm::vec3 kartCenter = kart.position + glm::vec3{0.f, 0.4f, 0.f};
			float kartRotation = -kart.heading + glm::radians(90.f);
			renderer::drawBox(kartCenter, {1.2f, 0.8f, 2.0f}, kart.color, kartRotation);
		}
	}

	void drawOverlay(int framebufferWidth, int framebufferHeight)
	{
		if (!gameData.debug.showOverlay)
		{
			return;
		}

		glm::vec3 phaseColor = {0.18f, 0.38f, 0.72f};
		if (gameData.race.phase == RacePhase::Countdown)
		{
			phaseColor = {0.92f, 0.58f, 0.18f};
		}
		else if (gameData.race.phase == RacePhase::Racing)
		{
			phaseColor = {0.18f, 0.62f, 0.34f};
		}
		else if (gameData.race.phase == RacePhase::Finished)
		{
			phaseColor = {0.72f, 0.22f, 0.18f};
		}

		drawRectPixels(framebufferHeight, framebufferWidth, {24, 18, framebufferWidth - 48, 14}, phaseColor);

		float countdownRatio = 1.f;
		if (gameData.race.phase == RacePhase::Countdown)
		{
			countdownRatio = glm::clamp(gameData.race.countdownTimer / 3.f, 0.f, 1.f);
		}
		else if (gameData.race.phase == RacePhase::Finished)
		{
			countdownRatio = glm::clamp(1.f - gameData.race.finishedTimer / 3.f, 0.f, 1.f);
		}
		drawRectPixels(framebufferHeight, framebufferWidth,
			{24, 38, static_cast<int>((framebufferWidth - 48) * countdownRatio), 8}, {0.92f, 0.9f, 0.9f});

		const KartState &playerKart = gameData.karts[gameData.race.playerKartIndex];
		float speedRatio = glm::clamp(playerKart.speed / 18.f, 0.f, 1.f);
		drawRectPixels(framebufferHeight, framebufferWidth, {24, framebufferHeight - 34, 160, 12}, {0.16f, 0.16f, 0.18f});
		drawRectPixels(framebufferHeight, framebufferWidth, {24, framebufferHeight - 34, static_cast<int>(160 * speedRatio), 12}, playerKart.color);

		float lapRatio = glm::clamp(
			(static_cast<float>(playerKart.progress.currentLap) + playerKart.progress.segmentProgress) /
			static_cast<float>(std::max(1, gameData.race.totalLaps)), 0.f, 1.f);
		drawRectPixels(framebufferHeight, framebufferWidth, {200, framebufferHeight - 34, 160, 12}, {0.16f, 0.16f, 0.18f});
		drawRectPixels(framebufferHeight, framebufferWidth, {200, framebufferHeight - 34, static_cast<int>(160 * lapRatio), 12}, {0.95f, 0.84f, 0.22f});

		for (int rank = 0; rank < static_cast<int>(gameData.race.ranking.size()); ++rank)
		{
			int kartIndex = gameData.race.ranking[rank];
			int barWidth = 14;
			int barHeight = 22 + std::max(0, 24 - rank * 3);
			int x = framebufferWidth - 30 - rank * 18;
			int y = 24;
			drawRectPixels(framebufferHeight, framebufferWidth, {x, y, barWidth, barHeight}, gameData.karts[kartIndex].color);
		}

		if (gameData.debug.eventFlashTimer > 0.f)
		{
			float flashRatio = glm::clamp(gameData.debug.eventFlashTimer / 0.35f, 0.f, 1.f);
			drawRectPixels(framebufferHeight, framebufferWidth,
				{framebufferWidth - 180, framebufferHeight - 34, static_cast<int>(156 * flashRatio), 12},
				{0.95f, 0.46f, 0.22f});
		}
	}
}

bool initGame()
{
	if (!renderer::init())
	{
		platform::log("Failed to initialize renderer", LogManager::logError);
		return false;
	}

	gameData = createDefaultGameState();
	platform::log("Init Phase 0.2 primitive renderer");

	return true;
}


//IMPORTANT NOTICE, IF YOU WANT TO SHIP THE GAME TO ANOTHER PC READ THE README.MD IN THE GITHUB
//https://github.com/meemknight/cmakeSetup
//OR THE INSTRUCTION IN THE CMAKE FILE.
//YOU HAVE TO CHANGE A FLAG IN THE CMAKE SO THAT RESOURCES_PATH POINTS TO RELATIVE PATHS
//BECAUSE OF SOME CMAKE PROGBLMS, RESOURCES_PATH IS SET TO BE ABSOLUTE DURING PRODUCTION FOR MAKING IT EASIER.

bool gameLogic(float deltaTime, platform::Input &input)
{
	int w = platform::getFrameBufferSizeX();
	int h = platform::getFrameBufferSizeY();

	processGameInput(gameData, input);

	float frameDt = glm::min(deltaTime, MAX_FRAME_TIME);
	simulationAccumulator += frameDt;
	while (simulationAccumulator >= FIXED_DT)
	{
		updateGameScaffold(gameData, FIXED_DT);
		simulationAccumulator -= FIXED_DT;
	}

	glViewport(0, 0, w, h);
	glClearColor(0.07f, 0.1f, 0.14f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderer::beginFrame(gameData.camera, w, h);
	drawWorld3D(gameData);

	glDisable(GL_DEPTH_TEST);
	glUseProgram(0);

	glEnable(GL_SCISSOR_TEST);
	drawOverlay(w, h);
	glDisable(GL_SCISSOR_TEST);

	return true;
}

//This function might not be be called if the program is forced closed
void closeGame()
{
	renderer::close();
	platform::log("Close Phase 0.2 primitive renderer");
}
