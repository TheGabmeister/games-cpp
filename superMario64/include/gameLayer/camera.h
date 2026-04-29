#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <platform/platformInput.h>
#include "input.h"

struct FlyCamera
{
	glm::vec3 position = {0.f, 3.f, 10.f};
	float yaw = -90.f;
	float pitch = -15.f;
	float speed = 10.f;
	float sensitivity = 0.1f;
	bool active = false;

	int lastMouseX = 0;
	int lastMouseY = 0;
	bool firstMouse = true;

	glm::mat4 getViewMatrix() const;
	glm::vec3 getFront() const;
	void update(platform::Input &input, float dt);
};

struct OrbitCamera
{
	float yaw = 0.f;
	float pitch = 25.f;
	float distance = 8.f;
	float sensitivity = 0.15f;
	float smoothing = 8.f;

	float autoCenterTimer = 0.f;
	float autoCenterDelay = 0.5f;
	float autoCenterSpeed = 2.f;

	glm::vec3 currentPosition = {};
	glm::vec3 currentTarget = {};

	glm::mat4 getViewMatrix() const;
	void update(const GameInput &input, glm::vec3 marioPos, float marioFacingAngle, float dt);
};

glm::mat4 getProjectionMatrix(float aspect);
