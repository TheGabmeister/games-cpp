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
#include <cmath>

#include "renderer.h"
#include "input.h"
#include "camera.h"
#include "mario.h"

gl2d::Renderer2D renderer2d;

Shader basicShader;
Shader lineShader;

FlyCamera flyCamera;
OrbitCamera orbitCamera;
bool useFlyCam = false;

Mario mario;
Mesh marioMesh;
bool marioMeshLoaded = false;

InputState inputState;

Mesh groundPlane;
Mesh cubes[5];
Mesh testModel;
bool testModelLoaded = false;

LineMesh gridMesh;
LineMesh axisMesh;

bool showGrid = true;
bool showAxes = true;
bool showDebugUI = true;

static constexpr float FIXED_DT = 1.f / 60.f;
static float accumulator = 0.f;
static GameInput currentInput = {};

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

	marioMesh = loadGLB(RESOURCES_PATH "models/mario.glb");
	marioMeshLoaded = (marioMesh.vao != 0);

	gridMesh = createGridMesh(50.f, 1.f);
	axisMesh = createAxisMesh(5.f);

	flyCamera.position = {0.f, 3.f, 10.f};
	flyCamera.yaw = -90.f;
	flyCamera.pitch = -15.f;

	// Initialize orbit camera behind Mario
	orbitCamera.currentPosition = {0.f, 4.f, 8.f};
	orbitCamera.currentTarget = {0.f, 1.f, 0.f};

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	return true;
}

static glm::vec3 getOrbitCameraForward()
{
	return glm::normalize(orbitCamera.currentTarget - orbitCamera.currentPosition);
}

bool gameLogic(float deltaTime, platform::Input &input)
{
	int w = platform::getFrameBufferSizeX();
	int h = platform::getFrameBufferSizeY();

	// Toggle keys
	if (input.isButtonPressed(platform::Button::NR1))
		showDebugUI = !showDebugUI;
	if (input.isButtonPressed(platform::Button::NR2))
	{
		useFlyCam = !useFlyCam;
		flyCamera.active = useFlyCam;
		platform::showMouse(!useFlyCam);
	}

	// Map input
	if (!useFlyCam && input.hasFocus)
		currentInput = mapInput(input, inputState);
	else
		currentInput = {};

	// Fly camera update (uses raw input, runs every frame)
	flyCamera.update(input, deltaTime);

	// Fixed timestep physics
	accumulator += deltaTime;
	if (accumulator > 0.1f) accumulator = 0.1f;

	while (accumulator >= FIXED_DT)
	{
		glm::vec3 camFwd = getOrbitCameraForward();
		mario.update(currentInput, FIXED_DT, camFwd);
		accumulator -= FIXED_DT;
	}

	// Orbit camera update (runs every frame for smooth following)
	if (!useFlyCam)
		orbitCamera.update(currentInput, mario.position, mario.facingAngle, deltaTime);

	// ---- Rendering ----
	glViewport(0, 0, w, h);
	glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float aspect = (h > 0) ? (float)w / (float)h : 1.f;
	glm::mat4 vp;
	if (useFlyCam)
		vp = getProjectionMatrix(aspect) * flyCamera.getViewMatrix();
	else
		vp = getProjectionMatrix(aspect) * orbitCamera.getViewMatrix();

	// Level geometry
	renderMesh(basicShader, groundPlane, glm::mat4(1.f), vp);
	for (int i = 0; i < 5; i++)
		renderMesh(basicShader, cubes[i], glm::translate(cubePositions[i]), vp);
	if (testModelLoaded)
		renderMesh(basicShader, testModel, glm::mat4(1.f), vp);

	// Mario
	if (marioMeshLoaded)
	{
		glm::mat4 marioModel = glm::translate(mario.position)
			* glm::rotate(glm::radians(mario.facingAngle + 180.f), glm::vec3(0, 1, 0));
		renderMesh(basicShader, marioMesh, marioModel, vp);
	}

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

		ImGui::Text("Mario State: %s", marioStateName(mario.state));
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", mario.position.x, mario.position.y, mario.position.z);
		glm::vec2 hVel(mario.velocity.x, mario.velocity.z);
		ImGui::Text("H Speed: %.2f  V Speed: %.2f", glm::length(hVel), mario.velocity.y);
		ImGui::Text("Facing: %.1f  OnGround: %s", mario.facingAngle, mario.onGround ? "yes" : "no");
		ImGui::Separator();

		ImGui::Text("Input: dir(%.2f,%.2f) str=%.2f", currentInput.moveDir.x, currentInput.moveDir.y, currentInput.moveStrength);
		ImGui::Text("Jump: %s  Held: %s", currentInput.jump ? "yes" : "no", currentInput.jumpHeld ? "yes" : "no");
		ImGui::Separator();

		ImGui::Checkbox("Grid", &showGrid);
		ImGui::SameLine();
		ImGui::Checkbox("Axes", &showAxes);
		ImGui::Text("Press 1: toggle debug UI");
		ImGui::Text("Press 2: fly camera (%s)", useFlyCam ? "ON" : "OFF");

		if (useFlyCam)
		{
			ImGui::Separator();
			ImGui::Text("Fly Cam: (%.1f, %.1f, %.1f)", flyCamera.position.x, flyCamera.position.y, flyCamera.position.z);
			ImGui::SliderFloat("Speed", &flyCamera.speed, 1.f, 50.f);
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
	if (marioMeshLoaded) destroyMesh(marioMesh);
	destroyLineMesh(gridMesh);
	destroyLineMesh(axisMesh);
	destroyShader(basicShader);
	destroyShader(lineShader);
}
