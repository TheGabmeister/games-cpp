#include "hud.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

void HudState::update(float dt)
{
	if (courseNameTimer > 0.f)
		courseNameTimer -= dt;
	lowHealthFlashTimer += dt;
}

void HudState::showCourseName(const std::string &course, const std::string &star)
{
	courseName = course;
	starName = star;
	courseNameTimer = COURSE_NAME_DURATION;
}

static void renderPowerMeter(gl2d::Renderer2D &renderer, glm::vec2 center, float radius,
	int health, int maxHealth, float flashTimer)
{
	renderer.renderCircleOutline(center, radius + 2.f, {0.05f, 0.05f, 0.05f, 0.7f}, 3.f, 32);

	for (int i = 0; i < maxHealth; i++)
	{
		float startAngle = (float)i * (360.f / (float)maxHealth) - 90.f;
		float endAngle = startAngle + (360.f / (float)maxHealth);

		gl2d::Color4f color;
		if (i < health)
		{
			if (health <= 2)
			{
				float pulse = (sinf(flashTimer * 10.f) + 1.f) * 0.5f;
				color = {1.f, pulse * 0.3f, 0.f, 1.f};
			}
			else
			{
				float t = (float)i / (float)(maxHealth - 1);
				color = {0.1f, 0.3f + t * 0.6f, 1.f - t * 0.4f, 1.f};
			}
		}
		else
		{
			color = {0.2f, 0.2f, 0.2f, 0.4f};
		}

		int linesPerSegment = 5;
		for (int j = 0; j <= linesPerSegment; j++)
		{
			float angle = glm::radians(startAngle + (endAngle - startAngle) * (float)j / (float)linesPerSegment);
			glm::vec2 tip = center + glm::vec2(cosf(angle), sinf(angle)) * radius;
			renderer.renderLine(center, tip, color, radius * 0.22f);
		}

		float gapAngle = glm::radians(endAngle);
		glm::vec2 gapEnd = center + glm::vec2(cosf(gapAngle), sinf(gapAngle)) * (radius + 1.f);
		renderer.renderLine(center, gapEnd, {0.05f, 0.05f, 0.05f, 0.8f}, 1.5f);
	}

	renderer.renderCircleOutline(center, radius + 2.f, {0.15f, 0.15f, 0.15f, 0.9f}, 2.f, 32);
}

void renderHud(gl2d::Renderer2D &renderer, gl2d::Font &font,
	int health, int maxHealth, int coins, int stars, int lives,
	float invincibilityTimer, const HudState &hud, int screenW, int screenH)
{
	// Power meter (top-left)
	glm::vec2 meterCenter = {70.f, 70.f};
	float meterRadius = 38.f;
	renderPowerMeter(renderer, meterCenter, meterRadius, health, maxHealth, hud.lowHealthFlashTimer);

	// Lives counter next to power meter
	char livesText[32];
	snprintf(livesText, sizeof(livesText), "x %d", lives);
	renderer.renderText({meterCenter.x + meterRadius + 16.f, meterCenter.y - 8.f},
		livesText, font, {1.f, 1.f, 1.f, 1.f}, 28.f, 4.f, 3.f, false);

	// Coin counter (top-right)
	float rightX = (float)screenW - 180.f;

	renderer.renderRectangle({rightX - 20.f, 22.f, 16.f, 16.f}, {1.f, 0.85f, 0.f, 1.f});
	char coinText[32];
	snprintf(coinText, sizeof(coinText), "x %d", coins);
	renderer.renderText({rightX, 18.f}, coinText, font, {1.f, 1.f, 1.f, 1.f}, 28.f, 4.f, 3.f, false);

	// Star counter (top-right, below coins)
	renderer.renderRectangle({rightX - 20.f, 54.f, 16.f, 16.f}, {1.f, 1.f, 0.f, 1.f});
	char starText[32];
	snprintf(starText, sizeof(starText), "x %d", stars);
	renderer.renderText({rightX, 50.f}, starText, font, {1.f, 1.f, 1.f, 1.f}, 28.f, 4.f, 3.f, false);

	// Course name (bottom-center, fading)
	if (hud.courseNameTimer > 0.f)
	{
		float alpha = 1.f;
		if (hud.courseNameTimer < HudState::COURSE_NAME_FADE_TIME)
			alpha = hud.courseNameTimer / HudState::COURSE_NAME_FADE_TIME;

		gl2d::Color4f textColor = {1.f, 1.f, 1.f, alpha};
		if (!hud.courseName.empty())
			renderer.renderText({(float)screenW * 0.5f, (float)screenH - 80.f},
				hud.courseName.c_str(), font, textColor, 36.f, 4.f, 3.f, true);
		if (!hud.starName.empty())
			renderer.renderText({(float)screenW * 0.5f, (float)screenH - 40.f},
				hud.starName.c_str(), font, textColor, 28.f, 4.f, 3.f, true);
	}
}
