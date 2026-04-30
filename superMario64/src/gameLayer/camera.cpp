#include "camera.h"
#include <cmath>
#include <algorithm>

// ---- Shared ----

glm::mat4 getProjectionMatrix(float aspect)
{
	return glm::perspective(glm::radians(45.f), aspect, 0.1f, 500.f);
}

// ---- FlyCamera ----

glm::vec3 FlyCamera::getFront() const
{
	float yr = glm::radians(yaw);
	float pr = glm::radians(pitch);
	return glm::normalize(glm::vec3(cos(yr) * cos(pr), sin(pr), sin(yr) * cos(pr)));
}

glm::mat4 FlyCamera::getViewMatrix() const
{
	glm::vec3 front = getFront();
	return glm::lookAt(position, position + front, glm::vec3(0, 1, 0));
}

void FlyCamera::update(platform::Input &input, float dt)
{
	if (!active || !input.hasFocus)
	{
		firstMouse = true;
		return;
	}

	int mx = input.mouseX;
	int my = input.mouseY;
	if (firstMouse)
	{
		lastMouseX = mx;
		lastMouseY = my;
		firstMouse = false;
	}

	yaw += (float)(mx - lastMouseX) * sensitivity;
	pitch += (float)(lastMouseY - my) * sensitivity;
	lastMouseX = mx;
	lastMouseY = my;

	if (pitch > 89.f) pitch = 89.f;
	if (pitch < -89.f) pitch = -89.f;

	glm::vec3 front = getFront();
	glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
	float s = speed * dt;

	if (input.isButtonHeld(platform::Button::W)) position += front * s;
	if (input.isButtonHeld(platform::Button::S)) position -= front * s;
	if (input.isButtonHeld(platform::Button::A)) position -= right * s;
	if (input.isButtonHeld(platform::Button::D)) position += right * s;
	if (input.isButtonHeld(platform::Button::Space)) position.y += s;
	if (input.isButtonHeld(platform::Button::LeftShift)) position.y -= s;
}

// ---- OrbitCamera ----

glm::mat4 OrbitCamera::getViewMatrix() const
{
	return glm::lookAt(currentPosition, currentTarget, glm::vec3(0, 1, 0));
}

void OrbitCamera::update(const GameInput &input, glm::vec3 marioPos, float marioFacingAngle, float dt, bool underwater)
{
	// Manual camera control
	bool manualInput = (std::abs(input.cameraDelta.x) > 0.5f || std::abs(input.cameraDelta.y) > 0.5f);
	if (manualInput)
	{
		yaw -= input.cameraDelta.x * sensitivity;
		pitch += input.cameraDelta.y * sensitivity;
		autoCenterTimer = 0.f;
	}
	else
	{
		autoCenterTimer += dt;
	}

	float pitchMin = underwater ? -30.f : 10.f;
	float pitchMax = underwater ? 85.f : 70.f;
	pitch = std::clamp(pitch, pitchMin, pitchMax);

	float currentDistance = underwater ? 10.f : distance;
	float currentSmoothing = underwater ? smoothing * 0.6f : smoothing;
	float currentAutoCenterSpeed = underwater ? autoCenterSpeed * 0.5f : autoCenterSpeed;

	// Auto-center behind Mario's facing direction
	if (autoCenterTimer > autoCenterDelay && input.moveStrength > 0.1f)
	{
		float targetYaw = 270.f - marioFacingAngle;

		float diff = targetYaw - yaw;
		while (diff > 180.f) diff -= 360.f;
		while (diff < -180.f) diff += 360.f;

		yaw += diff * currentAutoCenterSpeed * dt;
	}

	// Compute desired position from spherical coordinates
	glm::vec3 target = marioPos + glm::vec3(0.f, 1.0f, 0.f);

	float yr = glm::radians(yaw);
	float pr = glm::radians(pitch);
	glm::vec3 offset(
		cos(yr) * cos(pr) * currentDistance,
		sin(pr) * currentDistance,
		sin(yr) * cos(pr) * currentDistance
	);
	glm::vec3 desiredPos = target + offset;

	// Smooth interpolation
	float t = 1.f - exp(-currentSmoothing * dt);
	currentPosition = glm::mix(currentPosition, desiredPos, t);
	currentTarget = glm::mix(currentTarget, target, t);
}
