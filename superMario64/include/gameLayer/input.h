#pragma once
#include <glm/glm.hpp>
#include <platform/platformInput.h>

struct GameInput
{
	glm::vec2 moveDir = {0.f, 0.f};
	float moveStrength = 0.f;

	bool jump = false;
	bool jumpHeld = false;
	bool crouch = false;
	bool crouchHeld = false;
	bool attack = false;
	bool interact = false;
	bool pause = false;

	glm::vec2 cameraDelta = {0.f, 0.f};
};

struct InputState
{
	int prevMouseX = 0;
	int prevMouseY = 0;
	bool firstFrame = true;
};

GameInput mapInput(platform::Input &input, InputState &state);
