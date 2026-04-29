#include "input.h"
#include <cmath>

GameInput mapInput(platform::Input &input, InputState &state)
{
	GameInput gi = {};

	// Movement from keyboard (WASD)
	glm::vec2 kb(0.f);
	if (input.isButtonHeld(platform::Button::W)) kb.y += 1.f;
	if (input.isButtonHeld(platform::Button::S)) kb.y -= 1.f;
	if (input.isButtonHeld(platform::Button::A)) kb.x -= 1.f;
	if (input.isButtonHeld(platform::Button::D)) kb.x += 1.f;

	// Movement from gamepad left stick
	glm::vec2 pad(input.controller.LStick.x, -input.controller.LStick.y);

	// Use whichever has more magnitude
	float kbLen = glm::length(kb);
	float padLen = glm::length(pad);

	if (padLen > 0.15f && padLen > kbLen)
	{
		gi.moveDir = glm::normalize(pad);
		gi.moveStrength = glm::min(padLen, 1.f);
	}
	else if (kbLen > 0.f)
	{
		gi.moveDir = kb / kbLen;
		gi.moveStrength = 1.f;
	}

	// Actions
	gi.jump = input.isButtonPressed(platform::Button::Space) || input.controller.buttons[platform::Controller::A].pressed;
	gi.jumpHeld = input.isButtonHeld(platform::Button::Space) || input.controller.buttons[platform::Controller::A].held;
	gi.crouch = input.isButtonPressed(platform::Button::LeftCtrl) || input.controller.buttons[platform::Controller::LBumper].pressed;
	gi.crouchHeld = input.isButtonHeld(platform::Button::LeftCtrl) || input.controller.buttons[platform::Controller::LBumper].held;
	gi.attack = input.isButtonPressed(platform::Button::B) || input.controller.buttons[platform::Controller::X].pressed;
	gi.interact = input.isButtonPressed(platform::Button::E) || input.controller.buttons[platform::Controller::B].pressed;
	gi.pause = input.isButtonPressed(platform::Button::Escape) || input.controller.buttons[platform::Controller::Start].pressed;

	// Camera from mouse delta
	if (state.firstFrame)
	{
		state.prevMouseX = input.mouseX;
		state.prevMouseY = input.mouseY;
		state.firstFrame = false;
	}

	// Mouse camera: only when right mouse button is held
	if (input.isRMouseHeld())
	{
		gi.cameraDelta.x = (float)(input.mouseX - state.prevMouseX);
		gi.cameraDelta.y = (float)(state.prevMouseY - input.mouseY);
	}
	state.prevMouseX = input.mouseX;
	state.prevMouseY = input.mouseY;

	// Camera from gamepad right stick
	float rsX = input.controller.RStick.x;
	float rsY = input.controller.RStick.y;
	if (std::abs(rsX) > 0.15f || std::abs(rsY) > 0.15f)
	{
		gi.cameraDelta.x += rsX * 8.f;
		gi.cameraDelta.y += -rsY * 8.f;
	}

	return gi;
}
