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
#include "animation.h"
#include "world.h"
#include "hud.h"

gl2d::Renderer2D renderer2d;

Shader basicShader;
Shader lineShader;
SkinnedShader skinnedShader;

FlyCamera flyCamera;
OrbitCamera orbitCamera;
bool useFlyCam = false;

Mario mario;
SkinnedModel marioModel;
bool marioModelLoaded = false;
glm::mat4 marioSkinMatrices[MAX_BONES];

InputState inputState;

Mesh cubes[5];
Mesh testModel;
bool testModelLoaded = false;
Mesh phase4CollisionVisual;
bool phase4CollisionVisualLoaded = false;

CollisionWorld collisionWorld;
LineMesh collisionWireMesh;
bool collisionWorldLoaded = false;
bool collisionWireMeshLoaded = false;
bool showCollisionOverlay = true;
Phase5World phase5World;

LineMesh gridMesh;
LineMesh axisMesh;

Mesh yellowCoinMesh;
Mesh redCoinMesh;
Mesh blueCoinMesh;
Mesh heartMesh;

gl2d::Font hudFont;
HudState hudState;

bool showGrid = true;
bool showAxes = true;
bool showDebugUI = true;
bool showAnimDebugger = false;
bool animViewerMode = false;

static int frameCounter = 0;

static AnimState debugAnimState;
static bool debugAnimPaused = false;

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
	skinnedShader = loadSkinnedShader(RESOURCES_PATH "shaders/skinned.vert", RESOURCES_PATH "shaders/skinned.frag");

	for (int i = 0; i < 5; i++)
		cubes[i] = createCube(cubeColors[i]);

	testModel = loadGLB(RESOURCES_PATH "models/test_scene.glb");
	testModelLoaded = (testModel.vao != 0);
	phase4CollisionVisual = loadGLB(RESOURCES_PATH "courses/phase5_interactions.glb");
	phase4CollisionVisualLoaded = (phase4CollisionVisual.vao != 0);
	collisionWorldLoaded = loadCollisionWorldGLB(RESOURCES_PATH "courses/phase5_interactions.glb", collisionWorld);
	if (collisionWorldLoaded)
	{
		collisionWireMesh = createCollisionWireMesh(collisionWorld);
		collisionWireMeshLoaded = (collisionWireMesh.vao != 0);
	}
	initPhase5TestObjects(phase5World);
	initPhase6Collectibles(phase5World);

	yellowCoinMesh = createCube({1.f, 0.85f, 0.f});
	redCoinMesh = createCube({1.f, 0.2f, 0.2f});
	blueCoinMesh = createCube({0.2f, 0.4f, 1.f});
	heartMesh = createCube({1.f, 0.4f, 0.7f});

	hudFont.createFromFile(RESOURCES_PATH "roboto_black.ttf");
	hudState.showCourseName("Bob-omb Battlefield", "Star 1: Big Bob-omb on the Summit");

	marioModel = loadSkinnedGLB(RESOURCES_PATH "models/mario.glb");
	marioModelLoaded = (marioModel.mesh.vao != 0);
	if (marioModelLoaded)
		mario.setAnimClips(marioModel);

	// Initialize skin matrices to identity
	for (int i = 0; i < MAX_BONES; i++)
		marioSkinMatrices[i] = glm::mat4(1.f);

	gridMesh = createGridMesh(50.f, 1.f);
	axisMesh = createAxisMesh(5.f);

	flyCamera.position = {0.f, 3.f, 10.f};
	flyCamera.yaw = -90.f;
	flyCamera.pitch = -15.f;

	// Initialize orbit camera behind Mario
	orbitCamera.currentPosition = {0.f, 4.f, -8.f};
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
	if (input.isButtonPressed(platform::Button::NR4))
	{
		animViewerMode = !animViewerMode;
		if (animViewerMode)
			showAnimDebugger = true;
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
		if (!animViewerMode)
		{
			glm::vec3 camFwd = getOrbitCameraForward();
			updatePhase5Objects(phase5World, FIXED_DT, mario.position, mario.groundPoundImpact);
			mario.update(currentInput, FIXED_DT, camFwd, collisionWorldLoaded ? &collisionWorld : nullptr, &phase5World);

			updateCollectibles(phase5World, FIXED_DT);

			int pickup = checkCollectiblePickup(phase5World, mario.position);
			while (pickup >= 0)
			{
				switch (phase5World.collectibles[pickup].type)
				{
				case CollectibleType::YellowCoin: mario.collectCoin(1); break;
				case CollectibleType::RedCoin: mario.collectCoin(2); break;
				case CollectibleType::BlueCoin: mario.collectCoin(5); break;
				default: break;
				}
				pickup = checkCollectiblePickup(phase5World, mario.position);
			}

			if (checkSpinningHeart(phase5World, mario.position))
			{
				mario.healAccumulator += 4.f * FIXED_DT;
				if (mario.healAccumulator >= 1.f)
				{
					int segments = (int)mario.healAccumulator;
					mario.heal(segments);
					mario.healAccumulator -= (float)segments;
				}
			}
			else
			{
				mario.healAccumulator = 0.f;
			}

			hudState.update(FIXED_DT);

			if (marioModelLoaded)
				updateAnimState(mario.animState, marioModel.clips, FIXED_DT);
		}

		if (showAnimDebugger && marioModelLoaded && !debugAnimPaused && debugAnimState.currentClip >= 0)
			updateAnimState(debugAnimState, marioModel.clips, FIXED_DT);

		accumulator -= FIXED_DT;
	}

	// Orbit camera update (runs every frame for smooth following)
	if (!useFlyCam)
	{
		if (animViewerMode)
			orbitCamera.update(currentInput, glm::vec3(0.f), 0.f, deltaTime);
		else
			orbitCamera.update(currentInput, mario.position, mario.facingAngle, deltaTime);
	}

	// Evaluate animation for rendering
	if (marioModelLoaded)
	{
		if (showAnimDebugger && debugAnimState.currentClip >= 0)
			evaluateAnimState(debugAnimState, marioModel.clips, marioModel.skeleton, marioSkinMatrices);
		else
			evaluateAnimState(mario.animState, marioModel.clips, marioModel.skeleton, marioSkinMatrices);
	}

	// ---- Rendering ----
	glViewport(0, 0, w, h);
	if (animViewerMode)
		glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
	else
		glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	float aspect = (h > 0) ? (float)w / (float)h : 1.f;
	glm::mat4 vp;
	if (useFlyCam)
		vp = getProjectionMatrix(aspect) * flyCamera.getViewMatrix();
	else
		vp = getProjectionMatrix(aspect) * orbitCamera.getViewMatrix();

	// Level geometry
	if (!animViewerMode)
	{
		for (int i = 0; i < 5; i++)
			renderMesh(basicShader, cubes[i], glm::translate(cubePositions[i]), vp);
		if (testModelLoaded)
			renderMesh(basicShader, testModel, glm::mat4(1.f), vp);
		if (phase4CollisionVisualLoaded)
			renderMesh(basicShader, phase4CollisionVisual, glm::mat4(1.f), vp);
		for (const Phase5Object &object : phase5World.objects)
		{
			if (!object.active)
				continue;
			glm::mat4 model = glm::translate(object.position) *
				glm::scale(object.halfExtents * 2.f);
			renderMesh(basicShader, cubes[(int)object.type % 5], model, vp);
		}
		for (const Phase5Pole &pole : phase5World.poles)
		{
			glm::mat4 model = glm::translate(pole.base + glm::vec3(0.f, pole.height * 0.5f, 0.f)) *
				glm::scale(glm::vec3(pole.radius * 2.f, pole.height, pole.radius * 2.f));
			renderMesh(basicShader, cubes[3], model, vp);
		}

		// Collectibles
		for (const Collectible &c : phase5World.collectibles)
		{
			if (!c.active) continue;
			float bobY = sinf(c.bobTimer * 2.f) * 0.15f;
			glm::mat4 model = glm::translate(c.position + glm::vec3(0.f, bobY, 0.f))
				* glm::rotate(glm::radians(c.spinAngle), glm::vec3(0, 1, 0));
			switch (c.type)
			{
			case CollectibleType::YellowCoin:
				renderMesh(basicShader, yellowCoinMesh, model * glm::scale(glm::vec3(0.35f, 0.35f, 0.08f)), vp);
				break;
			case CollectibleType::RedCoin:
				renderMesh(basicShader, redCoinMesh, model * glm::scale(glm::vec3(0.4f, 0.4f, 0.08f)), vp);
				break;
			case CollectibleType::BlueCoin:
				renderMesh(basicShader, blueCoinMesh, model * glm::scale(glm::vec3(0.4f, 0.4f, 0.08f)), vp);
				break;
			case CollectibleType::SpinningHeart:
				renderMesh(basicShader, heartMesh, model * glm::scale(glm::vec3(0.5f, 0.5f, 0.12f)), vp);
				break;
			}
		}
	}

	// Mario (skinned) — flash during invincibility
	frameCounter++;
	bool renderMarioVisible = true;
	if (mario.isInvincible())
		renderMarioVisible = ((frameCounter / 4) % 2) == 0;

	if (marioModelLoaded && renderMarioVisible)
	{
		glm::mat4 marioModelMat;
		if (animViewerMode)
			marioModelMat = glm::mat4(1.f);
		else
			marioModelMat = glm::translate(mario.position)
				* glm::rotate(glm::radians(mario.facingAngle + 180.f), glm::vec3(0, 1, 0));
		renderSkinnedMesh(skinnedShader, marioModel.mesh, marioModelMat, vp,
			marioSkinMatrices, (int)marioModel.skeleton.bones.size());
	}

	// Debug overlays
	glLineWidth(2.f);
	if (showCollisionOverlay && collisionWireMeshLoaded) renderLines(lineShader, collisionWireMesh, vp);
	if (showGrid) renderLines(lineShader, gridMesh, vp);
	if (showAxes) renderLines(lineShader, axisMesh, vp);
	glLineWidth(1.f);

	// 2D HUD overlay
	if (!animViewerMode && !useFlyCam)
	{
		glDisable(GL_DEPTH_TEST);
		renderer2d.updateWindowMetrics(w, h);
		renderHud(renderer2d, hudFont, mario.health, mario.maxHealth,
			mario.coins, mario.stars, mario.lives, mario.invincibilityTimer,
			hudState, w, h);
		renderer2d.flush();
		glEnable(GL_DEPTH_TEST);
	}

	// ImGui
	if (showDebugUI && !animViewerMode)
	{
		ImGui::Begin("Debug");
		ImGui::Text("%.1f FPS (%.2f ms)", 1.f / deltaTime, deltaTime * 1000.f);
		ImGui::Separator();

		ImGui::Text("Mario State: %s", marioStateName(mario.state));
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", mario.position.x, mario.position.y, mario.position.z);
		glm::vec2 hVel(mario.velocity.x, mario.velocity.z);
		ImGui::Text("H Speed: %.2f  V Speed: %.2f", glm::length(hVel), mario.velocity.y);
		ImGui::Text("Facing: %.1f  OnGround: %s", mario.facingAngle, mario.onGround ? "yes" : "no");
		ImGui::Text("Ground: %s / %s  N(%.2f, %.2f, %.2f)",
			surfaceTypeName(mario.groundSurface), slopeClassName(mario.groundSlope),
			mario.groundNormal.x, mario.groundNormal.y, mario.groundNormal.z);
		ImGui::Text("JumpChain: %d  Timer: %.3f", mario.jumpChainCount, mario.jumpChainTimer);
		ImGui::Text("Buffer: %.3f  Coyote: %.3f", mario.jumpBufferTimer, mario.coyoteTimer);
		ImGui::Text("Combo: step=%d timer=%.3f", mario.punchComboStep, mario.comboTimer);
		ImGui::Separator();

		ImGui::Text("Anim: clip=%d time=%.2f blend=%.2f",
			mario.animState.currentClip, mario.animState.time, mario.animState.blendAlpha);
		ImGui::Separator();

		ImGui::Text("Input: dir(%.2f,%.2f) str=%.2f", currentInput.moveDir.x, currentInput.moveDir.y, currentInput.moveStrength);
		ImGui::Text("Jump: %s  Held: %s", currentInput.jump ? "yes" : "no", currentInput.jumpHeld ? "yes" : "no");
		ImGui::Text("Crouch: %s  Attack: %s", currentInput.crouchHeld ? "yes" : "no", currentInput.attack ? "yes" : "no");
		ImGui::Separator();

		ImGui::Checkbox("Grid", &showGrid);
		ImGui::SameLine();
		ImGui::Checkbox("Axes", &showAxes);
		ImGui::SameLine();
		ImGui::Checkbox("Collision", &showCollisionOverlay);
		ImGui::Text("Collision World: %s  tris=%d cells=%d",
			collisionWorldLoaded ? "loaded" : "missing",
			collisionWorld.triangleCount(), collisionWorld.occupiedCellCount());
		ImGui::Text("Phase5: objects=%d poles=%d carried=%d platform=%d wall=%.2f",
			(int)phase5World.objects.size(), (int)phase5World.poles.size(),
			mario.carriedObject, mario.currentPlatform, mario.wallContactTimer);
		ImGui::Separator();

		ImGui::Text("Health: %d/%d  Coins: %d  Stars: %d  Lives: %d",
			mario.health, mario.maxHealth, mario.coins, mario.stars, mario.lives);
		ImGui::Text("Invincible: %.2f  AirMaxY: %.2f",
			mario.invincibilityTimer, mario.airborneMaxY);
		if (ImGui::Button("Damage"))
			mario.takeDamage(1, mario.position + glm::vec3(1, 0, 0));
		ImGui::SameLine();
		if (ImGui::Button("Heal"))
			mario.heal(8);
		ImGui::SameLine();
		if (ImGui::Button("+Coin"))
			mario.collectCoin(1);
		ImGui::SameLine();
		if (ImGui::Button("Kill"))
			mario.die();
		ImGui::Separator();

		ImGui::Text("Press 1: toggle debug UI");
		ImGui::Text("Press 2: fly camera (%s)", useFlyCam ? "ON" : "OFF");
		ImGui::Text("Press 4: anim viewer (%s)", animViewerMode ? "ON" : "OFF");

		if (useFlyCam)
		{
			ImGui::Separator();
			ImGui::Text("Fly Cam: (%.1f, %.1f, %.1f)", flyCamera.position.x, flyCamera.position.y, flyCamera.position.z);
			ImGui::SliderFloat("Speed", &flyCamera.speed, 1.f, 50.f);
		}

		ImGui::End();
	}

	if (showAnimDebugger && marioModelLoaded)
	{
		ImGui::Begin("Animation Debugger", &showAnimDebugger);

		if (animViewerMode)
			ImGui::TextColored(ImVec4(0.5f, 1.f, 0.5f, 1.f), "VIEWER MODE (press 4 to exit)");

		const char *currentName = (debugAnimState.currentClip >= 0)
			? marioModel.clips[debugAnimState.currentClip].name.c_str()
			: "None";
		if (ImGui::BeginCombo("Clip", currentName))
		{
			for (int i = 0; i < (int)marioModel.clips.size(); i++)
			{
				bool selected = (i == debugAnimState.currentClip);
				if (ImGui::Selectable(marioModel.clips[i].name.c_str(), selected))
				{
					playClip(debugAnimState, i, 0.f);
					debugAnimPaused = false;
				}
			}
			ImGui::EndCombo();
		}

		if (debugAnimState.currentClip >= 0)
		{
			const AnimClip &clip = marioModel.clips[debugAnimState.currentClip];

			if (ImGui::Button(debugAnimPaused ? "Play" : "Pause"))
				debugAnimPaused = !debugAnimPaused;
			ImGui::SameLine();
			if (ImGui::Button("Restart"))
			{
				debugAnimState.time = 0.f;
				debugAnimPaused = false;
			}

			float t = debugAnimState.time;
			if (ImGui::SliderFloat("Time", &t, 0.f, clip.duration, "%.3f s"))
			{
				debugAnimState.time = t;
				debugAnimPaused = true;
			}

			ImGui::SliderFloat("Speed", &debugAnimState.speed, 0.1f, 3.f);

			ImGui::Separator();
			ImGui::Text("Duration: %.3f s", clip.duration);
			ImGui::Text("Looping: %s", clip.looping ? "yes" : "no");
			ImGui::Text("Channels: %d", (int)clip.channels.size());
		}

		if (ImGui::CollapsingHeader("Bones"))
		{
			for (int i = 0; i < (int)marioModel.skeleton.bones.size(); i++)
			{
				const Bone &b = marioModel.skeleton.bones[i];
				ImGui::Text("[%d] %s (parent: %d)", i, b.name.c_str(), b.parentIndex);
			}
		}

		ImGui::End();
	}

	return true;
}

void closeGame()
{
	for (auto &c : cubes) destroyMesh(c);
	destroyMesh(yellowCoinMesh);
	destroyMesh(redCoinMesh);
	destroyMesh(blueCoinMesh);
	destroyMesh(heartMesh);
	if (testModelLoaded) destroyMesh(testModel);
	if (phase4CollisionVisualLoaded) destroyMesh(phase4CollisionVisual);
	if (marioModelLoaded) destroySkinnedMesh(marioModel.mesh);
	if (collisionWireMeshLoaded) destroyLineMesh(collisionWireMesh);
	destroyLineMesh(gridMesh);
	destroyLineMesh(axisMesh);
	destroyShader(basicShader);
	destroyShader(lineShader);
	destroySkinnedShader(skinnedShader);
	hudFont.cleanup();
}
