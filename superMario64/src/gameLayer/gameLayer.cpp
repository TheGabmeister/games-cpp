#define GLM_ENABLE_EXPERIMENTAL
#include "gameLayer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "platformInput.h"
#include "imgui.h"
#include <gl2d/gl2d.h>
#include <platformTools.h>
#include <logs.h>

#include "renderer.h"

gl2d::Renderer2D renderer2d;

Shader basicShader;
Shader lineShader;
FlyCamera camera;

Mesh groundPlane;
Mesh cubes[5];
Mesh testModel;
bool testModelLoaded = false;

LineMesh gridMesh;
LineMesh axisMesh;

bool showGrid = true;
bool showAxes = true;
bool showDebugUI = true;

static const glm::vec3 cubePositions[5] = {
	{0.f, 0.5f, 0.f},
	{5.f, 0.5f, 0.f},
	{-5.f, 0.5f, 0.f},
	{0.f, 0.5f, 5.f},
	{0.f, 0.5f, -5.f},
};

static const glm::vec3 cubeColors[5] = {
	{1.f, 1.f, 1.f},
	{1.f, 0.2f, 0.2f},
	{0.5f, 0.1f, 0.1f},
	{0.2f, 0.2f, 1.f},
	{0.1f, 0.1f, 0.5f},
};

bool initGame()
{
	gl2d::init();
	renderer2d.create();

	basicShader = loadShader(RESOURCES_PATH "shaders/basic3d.vert", RESOURCES_PATH "shaders/basic3d.frag");
	lineShader = loadShader(RESOURCES_PATH "shaders/debug_line.vert", RESOURCES_PATH "shaders/debug_line.frag");

	groundPlane = createGroundPlane(50.f, {0.2f, 0.5f, 0.2f});
	for (int i = 0; i < 5; i++)
		cubes[i] = createCube(cubeColors[i]);

	testModel = loadGLB(RESOURCES_PATH "models/test_scene.glb");
	testModelLoaded = (testModel.vao != 0);

	gridMesh = createGridMesh(50.f, 1.f);
	axisMesh = createAxisMesh(5.f);

	camera.position = {0.f, 3.f, 10.f};
	camera.yaw = -90.f;
	camera.pitch = -15.f;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	platform::log("Phase 1 init complete");
	return true;
}

bool gameLogic(float deltaTime, platform::Input &input)
{
	int w = platform::getFrameBufferSizeX();
	int h = platform::getFrameBufferSizeY();

	glViewport(0, 0, w, h);
	glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Toggle keys: 1 = debug UI, 2 = fly camera
	if (input.isButtonPressed(platform::Button::NR1))
		showDebugUI = !showDebugUI;
	if (input.isButtonPressed(platform::Button::NR2))
	{
		camera.active = !camera.active;
		platform::showMouse(!camera.active);
	}

	camera.update(input, deltaTime);

	float aspect = (h > 0) ? (float)w / (float)h : 1.f;
	glm::mat4 vp = camera.getProjectionMatrix(aspect) * camera.getViewMatrix();

	// Render solid geometry
	renderMesh(basicShader, groundPlane, glm::mat4(1.f), vp);
	for (int i = 0; i < 5; i++)
		renderMesh(basicShader, cubes[i], glm::translate(cubePositions[i]), vp);
	if (testModelLoaded)
		renderMesh(basicShader, testModel, glm::mat4(1.f), vp);

	// Debug overlays
	glLineWidth(2.f);
	if (showGrid) renderLines(lineShader, gridMesh, vp);
	if (showAxes) renderLines(lineShader, axisMesh, vp);
	glLineWidth(1.f);

	// ImGui
	if (showDebugUI)
	{
		ImGui::Begin("Debug");
		ImGui::Text("%.1f FPS (%.2f ms)", 1.f / deltaTime, deltaTime * 1000.f);
		ImGui::Separator();
		ImGui::Text("Camera: (%.1f, %.1f, %.1f)", camera.position.x, camera.position.y, camera.position.z);
		ImGui::Text("Yaw: %.1f  Pitch: %.1f", camera.yaw, camera.pitch);
		ImGui::SliderFloat("Speed", &camera.speed, 1.f, 50.f);
		ImGui::SliderFloat("Sensitivity", &camera.sensitivity, 0.01f, 0.5f);
		ImGui::Separator();
		ImGui::Checkbox("Grid", &showGrid);
		ImGui::SameLine();
		ImGui::Checkbox("Axes", &showAxes);
		ImGui::Separator();
		ImGui::Text("Press 1: toggle debug UI");
		ImGui::Text("Press 2: toggle fly camera (%s)", camera.active ? "ON" : "OFF");
		if (ImGui::Button("Reset Camera"))
		{
			camera.position = {0.f, 3.f, 10.f};
			camera.yaw = -90.f;
			camera.pitch = -15.f;
		}
		ImGui::End();
	}

	return true;
}

void closeGame()
{
	destroyMesh(groundPlane);
	for (auto &c : cubes) destroyMesh(c);
	if (testModelLoaded) destroyMesh(testModel);
	destroyLineMesh(gridMesh);
	destroyLineMesh(axisMesh);
	destroyShader(basicShader);
	destroyShader(lineShader);
}
