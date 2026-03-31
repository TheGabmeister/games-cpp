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

	const char *getPlaceSuffix(int place)
	{
		if (place == 1) { return "ST"; }
		if (place == 2) { return "ND"; }
		if (place == 3) { return "RD"; }
		return "TH";
	}

	const char *getItemName(ItemType item)
	{
		switch (item)
		{
			case ItemType::Mushroom: return "MUSHROOM";
			case ItemType::Banana: return "BANANA";
			case ItemType::GreenShell: return "GREEN SHELL";
			case ItemType::RedShell: return "RED SHELL";
			default: return "";
		}
	}

	glm::vec4 getItemColor4(ItemType item)
	{
		switch (item)
		{
			case ItemType::Mushroom: return {0.95f, 0.4f, 0.3f, 1.f};
			case ItemType::Banana: return {0.95f, 0.85f, 0.15f, 1.f};
			case ItemType::GreenShell: return {0.2f, 0.85f, 0.3f, 1.f};
			case ItemType::RedShell: return {0.9f, 0.2f, 0.2f, 1.f};
			default: return {0.5f, 0.5f, 0.5f, 1.f};
		}
	}

	void drawHud(int w, int h)
	{
		if (!gameData.debug.showOverlay)
		{
			return;
		}

		const KartState &playerKart = gameData.karts[gameData.race.playerKartIndex];

		renderer2d.updateWindowMetrics(w, h);
		renderer2d.clearDrawData();

		char buf[64] = {};

		// Position (top-left)
		{
			int place = gameData.race.playerPlace;
			glm::vec4 placeColor = (place <= 1) ? glm::vec4(0.95f, 0.84f, 0.22f, 1.f)
				: (place <= 3) ? glm::vec4(0.78f, 0.78f, 0.82f, 1.f)
				: glm::vec4(0.65f, 0.45f, 0.3f, 1.f);

			std::snprintf(buf, sizeof(buf), "%d%s", place, getPlaceSuffix(place));
			glui::renderText(renderer2d, buf, menuFont,
				{16, 10, 80, 42}, placeColor, false, false);

			std::snprintf(buf, sizeof(buf), "/ %d", static_cast<int>(gameData.karts.size()));
			glui::renderText(renderer2d, buf, menuFont,
				{100, 22, 50, 22}, {0.6f, 0.6f, 0.6f, 1.f}, false, false);
		}

		// Lap counter (top-right)
		{
			int lap = std::min(playerKart.progress.currentLap + 1, gameData.race.totalLaps);
			std::snprintf(buf, sizeof(buf), "LAP %d/%d", lap, gameData.race.totalLaps);
			glui::renderText(renderer2d, buf, menuFont,
				{static_cast<float>(w - 160), 10, 144, 32}, {0.95f, 0.84f, 0.22f, 1.f}, false, true);
		}

		// Speed bar (bottom-left)
		{
			float speedRatio = glm::clamp(playerKart.speed / KART_MAX_SPEED, 0.f, 1.f);
			float barW = 160.f;
			float barH = 14.f;
			float barX = 20.f;
			float barY = h - 60.f;
			renderer2d.renderRectangle({barX, barY, barW, barH}, {0.16f, 0.16f, 0.18f, 1.f});
			renderer2d.renderRectangle({barX, barY, barW * speedRatio, barH},
				{playerKart.color.r, playerKart.color.g, playerKart.color.b, 1.f});

			std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::abs(playerKart.speed)));
			glui::renderText(renderer2d, buf, menuFont,
				{barX + barW + 8, barY - 2, 40, barH + 4}, {0.85f, 0.85f, 0.85f, 1.f}, false, false);
		}

		// Boost indicator (bottom-left, above speed bar)
		if (playerKart.boostTimer > 0.f)
		{
			glui::renderText(renderer2d, "BOOST", menuFont,
				{20, static_cast<float>(h - 80), 70, 18}, {0.95f, 0.55f, 0.1f, 1.f}, false, false);
		}

		// Drift indicator
		if (playerKart.driftState == DriftState::Drifting)
		{
			float driftAlpha = (playerKart.driftTimer >= MINI_TURBO_MIN_DRIFT_TIME)
				? 0.5f + 0.5f * std::sin(gameData.pulseTimer * 12.f) : 0.8f;
			glm::vec4 driftColor = (playerKart.driftTimer >= MINI_TURBO_MIN_DRIFT_TIME)
				? glm::vec4(0.95f, 0.55f, 0.1f, driftAlpha)
				: glm::vec4(0.6f, 0.6f, 0.65f, driftAlpha);
			glui::renderText(renderer2d, "DRIFT", menuFont,
				{20, static_cast<float>(h - 98), 60, 16}, driftColor, false, false);
		}

		// Held item (bottom-center)
		if (playerKart.heldItem != ItemType::None)
		{
			float itemBoxSize = 36.f;
			float itemX = w / 2.f - itemBoxSize / 2.f;
			float itemY = h - 56.f;
			glm::vec4 itemColor = getItemColor4(playerKart.heldItem);
			renderer2d.renderRectangle({itemX - 3, itemY - 3, itemBoxSize + 6, itemBoxSize + 6},
				{0.12f, 0.12f, 0.14f, 1.f});
			renderer2d.renderRectangle({itemX, itemY, itemBoxSize, itemBoxSize}, itemColor);

			glui::renderText(renderer2d, getItemName(playerKart.heldItem), menuFont,
				{w / 2.f - 60, itemY - 20, 120, 16}, itemColor, false, true);
		}

		// Countdown (center)
		if (gameData.race.phase == RacePhase::Countdown)
		{
			int countNum = static_cast<int>(std::ceil(gameData.race.countdownTimer));
			if (countNum > 0)
			{
				std::snprintf(buf, sizeof(buf), "%d", countNum);
				float scale = 1.f + 0.3f * (gameData.race.countdownTimer - std::floor(gameData.race.countdownTimer));
				float size = 80 * scale;
				glui::renderText(renderer2d, buf, menuFont,
					{w / 2.f - size / 2, h * 0.3f, size, size},
					{0.95f, 0.95f, 0.95f, 1.f}, false, true);
			}
		}
		else if (gameData.race.phase == RacePhase::Racing && gameData.race.raceTimer < 1.5f)
		{
			float alpha = 1.f - gameData.race.raceTimer / 1.5f;
			glui::renderText(renderer2d, "GO!", menuFont,
				{w / 2.f - 50, h * 0.3f, 100, 80},
				{0.18f, 0.85f, 0.34f, alpha}, false, true);
		}

		// Race timer (top-center)
		if (gameData.race.phase == RacePhase::Racing || gameData.race.phase == RacePhase::Finished)
		{
			int totalSeconds = static_cast<int>(gameData.race.raceTimer);
			int minutes = totalSeconds / 60;
			int seconds = totalSeconds % 60;
			int hundredths = static_cast<int>((gameData.race.raceTimer - totalSeconds) * 100);
			std::snprintf(buf, sizeof(buf), "%d:%02d.%02d", minutes, seconds, hundredths);
			glui::renderText(renderer2d, buf, menuFont,
				{w / 2.f - 60, 10, 120, 26}, {0.85f, 0.85f, 0.85f, 1.f}, false, true);
		}

		// Wrong-way warning (center)
		if (playerKart.wrongWay)
		{
			float flash = 0.5f + 0.5f * std::sin(gameData.pulseTimer * 8.f);
			glui::renderText(renderer2d, "WRONG WAY!", menuFont,
				{w / 2.f - 100, h * 0.22f, 200, 40},
				{0.95f, 0.2f, 0.2f, flash}, false, true);
		}

		// Respawn indicator
		if (playerKart.respawnTimer > 0.f)
		{
			float alpha = 0.5f + 0.5f * std::sin(gameData.pulseTimer * 10.f);
			glui::renderText(renderer2d, "RESPAWNING...", menuFont,
				{w / 2.f - 100, h * 0.55f, 200, 30},
				{0.85f, 0.85f, 0.9f, alpha}, false, true);
		}

		// Off-road indicator
		if (playerKart.offRoad && gameData.race.phase == RacePhase::Racing && !playerKart.wrongWay)
		{
			glui::renderText(renderer2d, "OFF ROAD", menuFont,
				{20, static_cast<float>(h - 116), 80, 14}, {0.55f, 0.4f, 0.2f, 0.8f}, false, false);
		}

		// Ranking column (top-right, below lap)
		{
			float colX = w - 30.f;
			float colY = 50.f;
			for (int rank = 0; rank < static_cast<int>(gameData.race.ranking.size()); ++rank)
			{
				int kartIndex = gameData.race.ranking[rank];
				float barHeight = 18.f + std::max(0.f, 16.f - rank * 2.f);
				glm::vec3 c = gameData.karts[kartIndex].color;
				renderer2d.renderRectangle(
					{colX - rank * 18.f, colY, 14.f, barHeight},
					{c.r, c.g, c.b, 1.f});
			}
		}

		renderer2d.flush();
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

		drawHud(w, h);

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
