#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct CameraState;

namespace renderer
{
	bool init();
	void close();

	void beginFrame(const CameraState &camera, int viewportWidth, int viewportHeight);

	void drawQuad(glm::vec3 center, glm::vec2 size, glm::vec3 color, float rotationY = 0.f);
	void drawBox(glm::vec3 center, glm::vec3 size, glm::vec3 color, float rotationY = 0.f);
	void drawLine(glm::vec3 start, glm::vec3 end, glm::vec3 color);
	void drawMarker(glm::vec3 position, float size, glm::vec3 color);
}
