#include "gameLayer.h"
#include "gameState.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <logs.h>

namespace
{
	GameState gameData = {};

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

	ScreenRect worldToScreenRect(glm::vec2 worldCenter, glm::vec2 worldSize, const TrackState &track,
		int panelX, int panelY, int panelWidth, int panelHeight)
	{
		glm::vec2 boundsSize = track.boundsMax - track.boundsMin;
		if (boundsSize.x <= 0.f || boundsSize.y <= 0.f)
		{
			return {};
		}

		auto mapPoint = [&](glm::vec2 point)
		{
			glm::vec2 normalized = (point - track.boundsMin) / boundsSize;
			return glm::vec2(
				panelX + normalized.x * panelWidth,
				panelY + (1.f - normalized.y) * panelHeight
			);
		};

		glm::vec2 minPoint = mapPoint(worldCenter - worldSize * 0.5f);
		glm::vec2 maxPoint = mapPoint(worldCenter + worldSize * 0.5f);

		ScreenRect rect = {};
		rect.x = static_cast<int>(std::round(std::min(minPoint.x, maxPoint.x)));
		rect.y = static_cast<int>(std::round(std::min(minPoint.y, maxPoint.y)));
		rect.width = static_cast<int>(std::round(std::abs(maxPoint.x - minPoint.x)));
		rect.height = static_cast<int>(std::round(std::abs(maxPoint.y - minPoint.y)));
		return rect;
	}

	void drawWorldPlaceholder(int framebufferWidth, int framebufferHeight)
	{
		int panelMargin = 36;
		int panelWidth = framebufferWidth - panelMargin * 2;
		int panelHeight = framebufferHeight - panelMargin * 2;
		int panelSize = std::max(180, std::min(panelWidth, panelHeight));
		int panelX = (framebufferWidth - panelSize) / 2;
		int panelY = (framebufferHeight - panelSize) / 2;

		drawRectPixels(framebufferHeight, framebufferWidth, {panelX, panelY, panelSize, panelSize}, {0.14f, 0.19f, 0.16f});

		const TrackState &track = gameData.track;
		float left = track.centerLine[0].x;
		float right = track.centerLine[1].x;
		float bottom = track.centerLine[0].z;
		float top = track.centerLine[2].z;
		float width = track.roadHalfWidth * 2.f;

		drawRectPixels(framebufferHeight, framebufferWidth,
			worldToScreenRect({0.f, bottom}, {right - left + width, width}, track, panelX, panelY, panelSize, panelSize),
			{0.22f, 0.23f, 0.27f});
		drawRectPixels(framebufferHeight, framebufferWidth,
			worldToScreenRect({0.f, top}, {right - left + width, width}, track, panelX, panelY, panelSize, panelSize),
			{0.22f, 0.23f, 0.27f});
		drawRectPixels(framebufferHeight, framebufferWidth,
			worldToScreenRect({left, 0.f}, {width, top - bottom + width}, track, panelX, panelY, panelSize, panelSize),
			{0.22f, 0.23f, 0.27f});
		drawRectPixels(framebufferHeight, framebufferWidth,
			worldToScreenRect({right, 0.f}, {width, top - bottom + width}, track, panelX, panelY, panelSize, panelSize),
			{0.22f, 0.23f, 0.27f});

		for (int i = 0; i < static_cast<int>(track.checkpoints.size()); ++i)
		{
			const Checkpoint &checkpoint = track.checkpoints[i];
			glm::vec2 center = {(checkpoint.start.x + checkpoint.end.x) * 0.5f, (checkpoint.start.z + checkpoint.end.z) * 0.5f};
			glm::vec2 size = {std::abs(checkpoint.end.x - checkpoint.start.x), std::abs(checkpoint.end.z - checkpoint.start.z)};
			size = glm::max(size, glm::vec2{2.f, 2.f});

			glm::vec3 color = (i == 0) ? glm::vec3{0.92f, 0.84f, 0.22f} : glm::vec3{0.84f, 0.48f, 0.2f};
			drawRectPixels(framebufferHeight, framebufferWidth,
				worldToScreenRect(center, size, track, panelX, panelY, panelSize, panelSize), color);
		}

		for (int i = 0; i < static_cast<int>(gameData.karts.size()); ++i)
		{
			const KartState &kart = gameData.karts[i];
			glm::vec2 center = {kart.position.x, kart.position.z};
			glm::vec2 size = (i == gameData.race.playerKartIndex) ? glm::vec2{4.f, 4.f} : glm::vec2{3.2f, 3.2f};

			if (i == gameData.race.playerKartIndex)
			{
				drawRectPixels(framebufferHeight, framebufferWidth,
					worldToScreenRect(center, size + glm::vec2{1.6f, 1.6f}, track, panelX, panelY, panelSize, panelSize),
					{0.98f, 0.98f, 0.98f});
			}

			drawRectPixels(framebufferHeight, framebufferWidth,
				worldToScreenRect(center, size, track, panelX, panelY, panelSize, panelSize),
				kart.color);
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
	gameData = createDefaultGameState();
	platform::log("Init Phase 0.1 gameplay scaffold");

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

	updateGameScaffold(gameData, deltaTime, input);

	glViewport(0, 0, w, h);
	glClearColor(0.07f, 0.1f, 0.14f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_SCISSOR_TEST);
	drawWorldPlaceholder(w, h);
	drawOverlay(w, h);
	glDisable(GL_SCISSOR_TEST);

	return true;
}

//This function might not be be called if the program is forced closed
void closeGame()
{
	platform::log("Close Phase 0.1 gameplay scaffold");
}
