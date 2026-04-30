#pragma once
#include <gl2d/gl2d.h>
#include <glm/glm.hpp>
#include <string>

struct HudState
{
	float courseNameTimer = 0.f;
	std::string courseName;
	std::string starName;
	float lowHealthFlashTimer = 0.f;

	static constexpr float COURSE_NAME_DURATION = 3.f;
	static constexpr float COURSE_NAME_FADE_TIME = 0.5f;

	void update(float dt);
	void showCourseName(const std::string &course, const std::string &star);
};

void renderHud(gl2d::Renderer2D &renderer, gl2d::Font &font,
	int health, int maxHealth, int coins, int stars, int lives,
	float invincibilityTimer, const HudState &hud, int screenW, int screenH);
