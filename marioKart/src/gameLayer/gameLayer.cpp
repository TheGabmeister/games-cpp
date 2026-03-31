#include "gameLayer.h"
#include "gameState.h"
#include "gameConfig.h"
#include "renderer.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <gl2d/gl2d.h>
#include <glui/glui.h>
#include <logs.h>

namespace
{
	GameState gameData = {};
	float simulationAccumulator = 0.f;

	gl2d::Renderer2D renderer2d;
	gl2d::Font menuFont;
	glui::RendererUi menuUi;

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

		// Wall guardrails
		for (size_t i = 0; i < track.centerLine.size(); ++i)
		{
			size_t next = (i + 1) % track.centerLine.size();
			glm::vec3 start = track.centerLine[i];
			glm::vec3 end = track.centerLine[next];
			glm::vec3 delta = end - start;
			float segLen = glm::length(delta);
			if (segLen <= 0.f) { continue; }

			glm::vec3 dir = delta / segLen;
			glm::vec3 right = {-dir.z, 0.f, dir.x};
			glm::vec3 center = (start + end) * 0.5f;
			float angle = std::atan2(delta.x, delta.z);

			glm::vec3 wallColorInner = {0.6f, 0.6f, 0.65f};
			glm::vec3 wallColorOuter = {0.45f, 0.15f, 0.15f};

			// Inner wall (road edge markers)
			for (int side = -1; side <= 1; side += 2)
			{
				glm::vec3 wallCenter = center + right * (track.wallHalfWidth * static_cast<float>(side));
				wallCenter.y = 0.4f;
				renderer::drawBox(wallCenter, {0.4f, 0.8f, segLen + 0.2f}, wallColorOuter, angle);
			}
		}

		// Finish line
		if (!track.checkpoints.empty())
		{
			const Checkpoint &startCp = track.checkpoints[0];
			glm::vec3 center = (startCp.start + startCp.end) * 0.5f;
			center.y = 0.02f;
			float span = glm::length(startCp.end - startCp.start);
			float angle = std::atan2(startCp.forward.x, startCp.forward.z);
			renderer::drawQuad(center, {1.5f, span}, {0.95f, 0.95f, 0.95f}, angle);
		}

		// Boost pads
		for (const BoostPad &pad : track.boostPads)
		{
			float angle = std::atan2(pad.direction.x, pad.direction.z);
			renderer::drawQuad(pad.position, {pad.halfWidth * 2.f, pad.halfLength * 2.f},
				{0.95f, 0.55f, 0.1f}, angle);
		}

		// Checkpoint markers
		for (int i = 1; i < static_cast<int>(track.checkpoints.size()); ++i)
		{
			const Checkpoint &cp = track.checkpoints[i];
			glm::vec3 center = (cp.start + cp.end) * 0.5f;
			center.y = 0.5f;
			renderer::drawMarker(center, 3.f, {0.84f, 0.48f, 0.2f});
		}

		// Item boxes
		for (const ItemBox &box : track.itemBoxes)
		{
			if (!box.active) { continue; }
			float bob = std::sin(game.pulseTimer * 3.f + box.position.x) * 0.3f;
			glm::vec3 pos = box.position + glm::vec3{0.f, bob, 0.f};
			renderer::drawBox(pos, {1.2f, 1.2f, 1.2f}, {0.9f, 0.9f, 0.2f}, game.pulseTimer * 1.5f);
		}

		// Hazards (bananas)
		for (const Hazard &haz : game.hazards)
		{
			if (!haz.alive) { continue; }
			renderer::drawBox(haz.position, {0.6f, 0.5f, 0.6f}, {0.95f, 0.85f, 0.15f});
		}

		// Projectiles
		for (const Projectile &proj : game.projectiles)
		{
			if (!proj.alive) { continue; }
			glm::vec3 color = (proj.type == ItemType::GreenShell)
				? glm::vec3(0.2f, 0.85f, 0.3f) : glm::vec3(0.9f, 0.2f, 0.2f);
			float angle = std::atan2(proj.velocity.x, proj.velocity.z);
			renderer::drawBox(proj.position, {0.7f, 0.5f, 0.7f}, color, angle);
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

	void drawMainMenu(int w, int h, const GameState &game)
	{
		renderer2d.updateWindowMetrics(w, h);
		renderer2d.clearDrawData();

		float centerX = w / 2.f;

		// Title
		glui::renderText(renderer2d, "KART RACER",
			menuFont, {centerX - 180, h * 0.15f, 360, 80},
			{1.f, 1.f, 1.f, 1.f}, false, true);

		// Color banner: row of kart palette swatches
		float swatchSize = 20.f;
		float gap = 8.f;
		float totalWidth = KART_PALETTE_COUNT * swatchSize + (KART_PALETTE_COUNT - 1) * gap;
		float startX = centerX - totalWidth / 2.f;
		for (int i = 0; i < KART_PALETTE_COUNT; ++i)
		{
			glm::vec3 c = getKartPaletteColor(i);
			float x = startX + i * (swatchSize + gap);
			renderer2d.renderRectangle({x, h * 0.35f, swatchSize, swatchSize},
				{c.r, c.g, c.b, 1.f});
		}

		// Pulsing "PRESS ENTER" prompt
		float pulse = 0.6f + 0.4f * std::sin(game.pulseTimer * MENU_PULSE_SPEED);
		glui::renderText(renderer2d, "PRESS ENTER",
			menuFont, {centerX - 140, h * 0.55f, 280, 50},
			{pulse, pulse, pulse, 1.f}, false, true);

		renderer2d.flush();
	}

	void drawKartSelect(int w, int h, const GameState &game)
	{
		renderer2d.updateWindowMetrics(w, h);
		renderer2d.clearDrawData();

		float centerX = w / 2.f;

		// Title
		glui::renderText(renderer2d, "SELECT KART",
			menuFont, {centerX - 160, h * 0.08f, 320, 60},
			{1.f, 1.f, 1.f, 1.f}, false, true);

		// Color palette strip
		float swatchSize = 36.f;
		float gap = 10.f;
		float totalWidth = KART_PALETTE_COUNT * swatchSize + (KART_PALETTE_COUNT - 1) * gap;
		float startX = centerX - totalWidth / 2.f;
		float paletteY = h * 0.25f;

		for (int i = 0; i < KART_PALETTE_COUNT; ++i)
		{
			glm::vec3 c = getKartPaletteColor(i);
			float x = startX + i * (swatchSize + gap);

			if (i == game.menu.selectedKartSlot)
			{
				renderer2d.renderRectangle(
					{x - 4, paletteY - 4, swatchSize + 8, swatchSize + 8},
					{1.f, 1.f, 1.f, 1.f});
			}

			renderer2d.renderRectangle({x, paletteY, swatchSize, swatchSize},
				{c.r, c.g, c.b, 1.f});
		}

		// Arrow indicators
		glui::renderText(renderer2d, "<",
			menuFont, {startX - 40, paletteY - 4, 30, swatchSize + 8},
			{0.8f, 0.8f, 0.8f, 1.f}, false, true);
		glui::renderText(renderer2d, ">",
			menuFont, {startX + totalWidth + 10, paletteY - 4, 30, swatchSize + 8},
			{0.8f, 0.8f, 0.8f, 1.f}, false, true);

		renderer2d.flush();

		// 3D kart preview
		{
			CameraState previewCam = {};
			previewCam.position = {3.f, 3.f, 3.f};
			previewCam.target = {0.f, 0.3f, 0.f};
			previewCam.distance = 5.f;
			previewCam.height = 3.f;

			renderer::beginFrame(previewCam, w, h);
			glm::vec3 kartColor = getKartPaletteColor(game.menu.selectedKartSlot);
			renderer::drawBox({0.f, 0.4f, 0.f}, {1.2f, 0.8f, 2.0f},
				kartColor, game.menu.previewRotation);
			// Ground under kart
			renderer::drawQuad({0.f, -0.01f, 0.f}, {5.f, 5.f}, {0.15f, 0.15f, 0.18f});
		}

		// Back to 2D for confirm prompt
		glDisable(GL_DEPTH_TEST);
		glUseProgram(0);

		renderer2d.clearDrawData();

		float pulse = 0.5f + 0.5f * std::sin(game.pulseTimer * MENU_PULSE_SPEED);
		glui::renderText(renderer2d, "ENTER TO CONFIRM",
			menuFont, {centerX - 160, h * 0.82f, 320, 40},
			{0.95f * pulse, 0.84f * pulse, 0.22f * pulse, 1.f}, false, true);

		glui::renderText(renderer2d, "ESC TO GO BACK",
			menuFont, {centerX - 130, h * 0.9f, 260, 30},
			{0.5f, 0.5f, 0.5f, 1.f}, false, true);

		renderer2d.flush();
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

		// Player place indicator (stacked dots: fewer = better)
		{
			int place = gameData.race.playerPlace;
			int totalKarts = static_cast<int>(gameData.karts.size());
			glm::vec3 placeColor = (place <= 1) ? glm::vec3(0.95f, 0.84f, 0.22f)
				: (place <= 3) ? glm::vec3(0.7f, 0.7f, 0.75f)
				: glm::vec3(0.55f, 0.35f, 0.2f);
			int dotSize = 8;
			int spacing = 12;
			int startX = framebufferWidth / 2 - (place * spacing) / 2;
			int y = 20;
			for (int p = 0; p < place; ++p)
			{
				drawRectPixels(framebufferHeight, framebufferWidth,
					{startX + p * spacing, y, dotSize, dotSize}, placeColor);
			}

			// Background bar showing total field size
			int totalWidth = totalKarts * spacing;
			int bgX = framebufferWidth / 2 - totalWidth / 2;
			drawRectPixels(framebufferHeight, framebufferWidth,
				{bgX, y + dotSize + 2, totalWidth, 3}, {0.2f, 0.2f, 0.22f});
			int filledWidth = place * spacing;
			int fillX = framebufferWidth / 2 - totalWidth / 2;
			drawRectPixels(framebufferHeight, framebufferWidth,
				{fillX, y + dotSize + 2, filledWidth, 3}, placeColor);
		}

		if (gameData.debug.eventFlashTimer > 0.f)
		{
			float flashRatio = glm::clamp(gameData.debug.eventFlashTimer / 0.35f, 0.f, 1.f);
			drawRectPixels(framebufferHeight, framebufferWidth,
				{framebufferWidth - 180, framebufferHeight - 34, static_cast<int>(156 * flashRatio), 12},
				{0.95f, 0.46f, 0.22f});
		}

		// Wrong-way warning
		if (playerKart.wrongWay)
		{
			int barW = framebufferWidth - 48;
			drawRectPixels(framebufferHeight, framebufferWidth,
				{24, 56, barW, 10}, {0.95f, 0.2f, 0.2f});
		}

		// Off-road indicator
		if (playerKart.offRoad && gameData.race.phase == RacePhase::Racing)
		{
			drawRectPixels(framebufferHeight, framebufferWidth,
				{24, framebufferHeight - 50, 80, 8}, {0.5f, 0.35f, 0.15f});
		}

		// Held item indicator
		if (playerKart.heldItem != ItemType::None)
		{
			glm::vec3 itemColor = {0.5f, 0.5f, 0.5f};
			int itemWidth = 24;
			if (playerKart.heldItem == ItemType::Mushroom) { itemColor = {0.95f, 0.4f, 0.3f}; }
			else if (playerKart.heldItem == ItemType::Banana) { itemColor = {0.95f, 0.85f, 0.15f}; }
			else if (playerKart.heldItem == ItemType::GreenShell) { itemColor = {0.2f, 0.85f, 0.3f}; }
			else if (playerKart.heldItem == ItemType::RedShell) { itemColor = {0.9f, 0.2f, 0.2f}; }
			int itemX = framebufferWidth / 2 - itemWidth / 2;
			int itemY = framebufferHeight - 50;
			drawRectPixels(framebufferHeight, framebufferWidth,
				{itemX - 2, itemY - 2, itemWidth + 4, itemWidth + 4}, {0.12f, 0.12f, 0.14f});
			drawRectPixels(framebufferHeight, framebufferWidth,
				{itemX, itemY, itemWidth, itemWidth}, itemColor);
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

	gl2d::init();
	renderer2d.create();
	menuFont.createFromFile(RESOURCES_PATH "roboto_black.ttf");

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

	if (gameData.race.phase == RacePhase::MainMenu)
	{
		drawMainMenu(w, h, gameData);
	}
	else if (gameData.race.phase == RacePhase::KartSelect)
	{
		drawKartSelect(w, h, gameData);
	}
	else
	{
		renderer::beginFrame(gameData.camera, w, h);
		drawWorld3D(gameData);

		glDisable(GL_DEPTH_TEST);
		glUseProgram(0);

		glEnable(GL_SCISSOR_TEST);
		drawOverlay(w, h);
		glDisable(GL_SCISSOR_TEST);

		if (gameData.race.phase == RacePhase::Finished)
		{
			renderer2d.updateWindowMetrics(w, h);
			renderer2d.clearDrawData();
			float pulse = 0.5f + 0.5f * std::sin(gameData.pulseTimer * MENU_PULSE_SPEED);
			float cx = w / 2.f;
			glui::renderText(renderer2d, "RACE FINISHED!",
				menuFont, {cx - 160, h * 0.35f, 320, 60},
				{0.95f, 0.84f, 0.22f, 1.f}, false, true);
			glui::renderText(renderer2d, "PRESS ENTER",
				menuFont, {cx - 130, h * 0.48f, 260, 40},
				{pulse, pulse, pulse, 1.f}, false, true);
			renderer2d.flush();
		}
	}

	return true;
}

//This function might not be be called if the program is forced closed
void closeGame()
{
	menuFont.cleanup();
	renderer2d.cleanup();
	gl2d::cleanup();
	renderer::close();
	platform::log("Close Phase 0.2 primitive renderer");
}
