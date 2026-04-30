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
#include "entity.h"
#include "entityManager.h"
#include "course.h"

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
Mesh starMesh;
Mesh enemyMesh;
Mesh waterPlaneMesh;

SkinnedModel enemyModels[(int)EntityType::COUNT];
bool enemyModelsLoaded[(int)EntityType::COUNT] = {};
EntityManager entityManager;
LoadedCourse activeCourse;
bool useCourse = false;

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
	initTestEnemies(phase5World);
	initPhase7Water(phase5World);

	waterPlaneMesh = createGroundPlane(1.f, {0.2f, 0.4f, 0.85f});

	yellowCoinMesh = createCube({1.f, 0.85f, 0.f});
	redCoinMesh = createCube({1.f, 0.2f, 0.2f});
	blueCoinMesh = createCube({0.2f, 0.4f, 1.f});
	heartMesh = createCube({1.f, 0.4f, 0.7f});
	starMesh = createCube({1.f, 1.f, 0.2f});
	enemyMesh = createCube({0.8f, 0.15f, 0.1f});

	enemyModels[(int)EntityType::Goomba] = loadSkinnedGLB(RESOURCES_PATH "models/goomba.glb");
	enemyModels[(int)EntityType::BobOmb] = loadSkinnedGLB(RESOURCES_PATH "models/bob_omb.glb");
	enemyModels[(int)EntityType::KoopaTroopa] = loadSkinnedGLB(RESOURCES_PATH "models/koopa_troopa.glb");
	enemyModels[(int)EntityType::Boo] = loadSkinnedGLB(RESOURCES_PATH "models/boo.glb");
	for (int i = 0; i < (int)EntityType::COUNT; i++)
		enemyModelsLoaded[i] = (enemyModels[i].mesh.vao != 0);

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
	if (input.isButtonPressed(platform::Button::NR5))
	{
		if (!useCourse)
		{
			if (activeCourse.loadFromJSON(RESOURCES_PATH "courses/bob_omb_battlefield.json"))
			{
				activeCourse.spawnEntities();
				mario.position = activeCourse.def.marioSpawn;
				mario.velocity = {};
				mario.spawnPosition = activeCourse.def.marioSpawn;
				useCourse = true;
			}
		}
		else
		{
			activeCourse.unload();
			useCourse = false;
			mario.position = {0.f, 0.f, 0.f};
			mario.velocity = {};
			mario.spawnPosition = {0.f, 0.f, 0.f};
		}
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

			CollisionWorld *activeCollWorld = nullptr;
			Phase5World *activeP5World = nullptr;
			EntityManager *activeEntMgr = nullptr;
			if (useCourse && activeCourse.loaded)
			{
				activeCollWorld = &activeCourse.collisionWorld;
				activeP5World = &activeCourse.phase5World;
				activeEntMgr = &activeCourse.entityManager;
			}
			else
			{
				activeCollWorld = collisionWorldLoaded ? &collisionWorld : nullptr;
				activeP5World = &phase5World;
				activeEntMgr = &entityManager;
			}

			updatePhase5Objects(*activeP5World, FIXED_DT, mario.position, mario.groundPoundImpact);
			mario.update(currentInput, FIXED_DT, camFwd, activeCollWorld, activeP5World);

			updateCollectibles(*activeP5World, FIXED_DT);

			int pickup = checkCollectiblePickup(*activeP5World, mario.position);
			while (pickup >= 0)
			{
				CollectibleType ct = activeP5World->collectibles[pickup].type;
				switch (ct)
				{
				case CollectibleType::YellowCoin: mario.collectCoin(1); break;
				case CollectibleType::RedCoin:
					mario.collectCoin(2);
					if (useCourse) activeCourse.redCoinsCollected++;
					break;
				case CollectibleType::BlueCoin: mario.collectCoin(5); break;
				case CollectibleType::Star:
					mario.stars++;
					break;
				default: break;
				}
				pickup = checkCollectiblePickup(*activeP5World, mario.position);
			}

			if (checkSpinningHeart(*activeP5World, mario.position))
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

			// Old test enemies
			int enemyHit = checkEnemyContact(*activeP5World, mario.position, Mario::COLLISION_RADIUS);
			if (enemyHit >= 0)
				mario.takeDamage(1, activeP5World->enemies[enemyHit].position);

			// Entity system enemies
			activeEntMgr->spawnDespawn(mario.position);
			activeEntMgr->update(FIXED_DT, mario.position, mario.facingAngle, activeCollWorld);

			// Combat: stomp > attack > contact
			if (mario.isStomping())
			{
				int target = activeEntMgr->checkAttackHit(mario.position, Mario::COLLISION_RADIUS + 0.2f);
				if (target >= 0)
				{
					Entity *e = activeEntMgr->entities[target].get();
					activeEntMgr->defeatEnemy(target);
					mario.velocity.y = Mario::JUMP_VELOCITY * 0.6f;
					if (e->dropsCoin)
					{
						Collectible coin;
						coin.type = (e->coinDropValue >= 5) ? CollectibleType::BlueCoin : CollectibleType::YellowCoin;
						coin.position = e->position + glm::vec3(0.f, 0.5f, 0.f);
						activeP5World->collectibles.push_back(coin);
					}
				}
			}
			else if (mario.isAttacking())
			{
				glm::vec3 attackPos = mario.position + mario.getFacingForward() * 0.5f;
				int target = activeEntMgr->checkAttackHit(attackPos, 1.0f);
				if (target >= 0)
				{
					Entity *e = activeEntMgr->entities[target].get();
					activeEntMgr->defeatEnemy(target);
					if (e->dropsCoin)
					{
						Collectible coin;
						coin.type = (e->coinDropValue >= 5) ? CollectibleType::BlueCoin : CollectibleType::YellowCoin;
						coin.position = e->position + glm::vec3(0.f, 0.5f, 0.f);
						activeP5World->collectibles.push_back(coin);
					}
				}
			}
			else if (!mario.isInvincible() && !mario.isDead())
			{
				int contact = activeEntMgr->checkEnemyContact(mario.position, Mario::COLLISION_RADIUS);
				if (contact >= 0)
				{
					Entity *e = activeEntMgr->entities[contact].get();
					mario.takeDamage(1, e->position);
				}
			}

			// Update entity animations
			for (auto &entity : activeEntMgr->entities)
			{
				if (!entity->active || !entity->alive) continue;
				int mi = (int)entity->type;
				if (mi < (int)EntityType::COUNT && enemyModelsLoaded[mi])
					updateAnimState(entity->animState, enemyModels[mi].clips, FIXED_DT);
			}

			// Red coin star check
			if (useCourse && activeCourse.redCoinsCollected >= 8)
			{
				// Spawn star at a fixed location (could be configurable)
				bool alreadyHasStar = false;
				for (const Collectible &c : activeP5World->collectibles)
					if (c.type == CollectibleType::Star && c.active) alreadyHasStar = true;
				if (!alreadyHasStar)
				{
					Collectible star;
					star.type = CollectibleType::Star;
					star.position = mario.position + glm::vec3(0.f, 3.f, 0.f);
					star.radius = 2.0f;
					activeP5World->collectibles.push_back(star);
				}
			}

			// 100-coin star
			if (useCourse && mario.coins >= 100 && !activeCourse.hundredCoinStarSpawned)
			{
				activeCourse.hundredCoinStarSpawned = true;
				Collectible star;
				star.type = CollectibleType::Star;
				star.position = mario.position + glm::vec3(0.f, 3.f, 0.f);
				star.radius = 2.0f;
				activeP5World->collectibles.push_back(star);
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
		bool marioUnderwater = (mario.state == MarioState::SWIMMING_UNDERWATER);
		if (animViewerMode)
			orbitCamera.update(currentInput, glm::vec3(0.f), 0.f, deltaTime);
		else
			orbitCamera.update(currentInput, mario.position, mario.facingAngle, deltaTime, marioUnderwater);
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
	Phase5World &renderP5World = (useCourse && activeCourse.loaded) ? activeCourse.phase5World : phase5World;
	EntityManager &renderEntMgr = (useCourse && activeCourse.loaded) ? activeCourse.entityManager : entityManager;

	glViewport(0, 0, w, h);
	bool cameraUnderwater = false;
	if (!animViewerMode)
	{
		glm::vec3 camPos = useFlyCam ? flyCamera.position : orbitCamera.currentPosition;
		for (const WaterVolume &wv : renderP5World.waterVolumes)
		{
			if (camPos.x >= wv.minBounds.x && camPos.x <= wv.maxBounds.x &&
				camPos.z >= wv.minBounds.z && camPos.z <= wv.maxBounds.z &&
				camPos.y <= wv.surfaceY)
			{
				cameraUnderwater = true;
				break;
			}
		}
	}

	if (cameraUnderwater)
		glClearColor(0.05f, 0.15f, 0.35f, 1.0f);
	else if (animViewerMode)
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
		if (useCourse && activeCourse.loaded)
		{
			if (activeCourse.visualMesh.vao)
				renderMesh(basicShader, activeCourse.visualMesh, glm::mat4(1.f), vp);
		}
		else
		{
			for (int i = 0; i < 5; i++)
				renderMesh(basicShader, cubes[i], glm::translate(cubePositions[i]), vp);
			if (testModelLoaded)
				renderMesh(basicShader, testModel, glm::mat4(1.f), vp);
			if (phase4CollisionVisualLoaded)
				renderMesh(basicShader, phase4CollisionVisual, glm::mat4(1.f), vp);
		}

		for (const Phase5Object &object : renderP5World.objects)
		{
			if (!object.active)
				continue;
			glm::mat4 model = glm::translate(object.position) *
				glm::scale(object.halfExtents * 2.f);
			renderMesh(basicShader, cubes[(int)object.type % 5], model, vp);
		}
		for (const Phase5Pole &pole : renderP5World.poles)
		{
			glm::mat4 model = glm::translate(pole.base + glm::vec3(0.f, pole.height * 0.5f, 0.f)) *
				glm::scale(glm::vec3(pole.radius * 2.f, pole.height, pole.radius * 2.f));
			renderMesh(basicShader, cubes[3], model, vp);
		}

		// Collectibles
		for (const Collectible &c : renderP5World.collectibles)
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
			case CollectibleType::Star:
				renderMesh(basicShader, starMesh, model * glm::scale(glm::vec3(0.6f, 0.6f, 0.15f)), vp);
				break;
			}
		}

		// Test enemies (old system)
		for (const TestEnemy &e : renderP5World.enemies)
		{
			if (!e.active) continue;
			glm::mat4 model = glm::translate(e.position) * glm::scale(glm::vec3(e.radius));
			renderMesh(basicShader, enemyMesh, model, vp);
		}

		// Entity system enemies (skinned models)
		for (auto &entity : renderEntMgr.entities)
		{
			if (!entity->active || !entity->alive) continue;
			int mi = (int)entity->type;
			if (mi >= (int)EntityType::COUNT || !enemyModelsLoaded[mi])
			{
				// Fallback: render as colored cube
				glm::mat4 model = glm::translate(entity->position)
					* glm::scale(glm::vec3(entity->collisionRadius));
				renderMesh(basicShader, enemyMesh, model, vp);
				continue;
			}

			SkinnedModel &eModel = enemyModels[mi];
			glm::mat4 eSkinMatrices[MAX_BONES];
			evaluateAnimState(entity->animState, eModel.clips, eModel.skeleton, eSkinMatrices);

			glm::mat4 entityModelMat = glm::translate(entity->position)
				* glm::rotate(glm::radians(entity->facingAngle + 180.f), glm::vec3(0, 1, 0));

			bool needBlend = (entity->transparency > 0.01f);
			if (needBlend)
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glUseProgram(skinnedShader.program);
				if (skinnedShader.u_alpha >= 0)
					glUniform1f(skinnedShader.u_alpha, 1.f - entity->transparency);
			}

			renderSkinnedMesh(skinnedShader, eModel.mesh, entityModelMat, vp,
				eSkinMatrices, (int)eModel.skeleton.bones.size());

			if (needBlend)
			{
				if (skinnedShader.u_alpha >= 0)
					glUniform1f(skinnedShader.u_alpha, 1.0f);
				glDisable(GL_BLEND);
			}
		}

		// Water planes (transparent)
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		glUseProgram(basicShader.program);
		if (basicShader.u_alpha >= 0)
			glUniform1f(basicShader.u_alpha, 0.45f);
		for (const WaterVolume &wv : renderP5World.waterVolumes)
		{
			glm::vec3 center = (wv.minBounds + wv.maxBounds) * 0.5f;
			center.y = wv.surfaceY;
			float sizeX = wv.maxBounds.x - wv.minBounds.x;
			float sizeZ = wv.maxBounds.z - wv.minBounds.z;
			glm::mat4 model = glm::translate(center) * glm::scale(glm::vec3(sizeX, 1.f, sizeZ));
			renderMesh(basicShader, waterPlaneMesh, model, vp);
		}
		if (basicShader.u_alpha >= 0)
			glUniform1f(basicShader.u_alpha, 1.0f);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
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
	if (showCollisionOverlay)
	{
		if (useCourse && activeCourse.loaded && activeCourse.collisionWireMesh.vao)
			renderLines(lineShader, activeCourse.collisionWireMesh, vp);
		else if (collisionWireMeshLoaded)
			renderLines(lineShader, collisionWireMesh, vp);
	}
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
		ImGui::Text("Invincible: %.2f  AirMaxY: %.2f  Water: %d",
			mario.invincibilityTimer, mario.airborneMaxY, mario.currentWaterVolume);
		if (mario.state == MarioState::SWIMMING_SURFACE || mario.state == MarioState::SWIMMING_UNDERWATER)
			ImGui::Text("AirTimer: %.1f  SwimPitch: %.1f  PreHealth: %d",
				mario.airTimer, mario.swimPitch, mario.preSwimHealth);
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

		// Entity system debug
		EntityManager &dbgEntMgr = (useCourse && activeCourse.loaded) ? activeCourse.entityManager : entityManager;
		int activeEnts = 0;
		for (auto &e : dbgEntMgr.entities)
			if (e->active && e->alive) activeEnts++;
		ImGui::Text("Entities: %d/%d active", activeEnts, (int)dbgEntMgr.entities.size());
		if (useCourse)
			ImGui::Text("Course: %s  RedCoins: %d/8", activeCourse.def.name.c_str(), activeCourse.redCoinsCollected);
		ImGui::Separator();

		ImGui::Text("Press 1: toggle debug UI");
		ImGui::Text("Press 2: fly camera (%s)", useFlyCam ? "ON" : "OFF");
		ImGui::Text("Press 4: anim viewer (%s)", animViewerMode ? "ON" : "OFF");
		ImGui::Text("Press 5: toggle course (%s)", useCourse ? "BoB" : "Test Level");

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
	destroyMesh(enemyMesh);
	destroyMesh(starMesh);
	destroyMesh(waterPlaneMesh);
	if (testModelLoaded) destroyMesh(testModel);
	if (phase4CollisionVisualLoaded) destroyMesh(phase4CollisionVisual);
	if (marioModelLoaded) destroySkinnedMesh(marioModel.mesh);
	for (int i = 0; i < (int)EntityType::COUNT; i++)
		if (enemyModelsLoaded[i]) destroySkinnedMesh(enemyModels[i].mesh);
	if (activeCourse.loaded) activeCourse.unload();
	if (collisionWireMeshLoaded) destroyLineMesh(collisionWireMesh);
	destroyLineMesh(gridMesh);
	destroyLineMesh(axisMesh);
	destroyShader(basicShader);
	destroyShader(lineShader);
	destroySkinnedShader(skinnedShader);
	hudFont.cleanup();
}
