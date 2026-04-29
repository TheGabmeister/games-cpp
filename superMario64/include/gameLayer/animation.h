#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <unordered_map>

static constexpr int MAX_BONES = 64;

struct Bone
{
	std::string name;
	int parentIndex = -1;
	glm::mat4 inverseBindMatrix = glm::mat4(1.f);
	glm::vec3 localPosition = {0.f, 0.f, 0.f};
	glm::quat localRotation = glm::quat(1.f, 0.f, 0.f, 0.f);
	glm::vec3 localScale = {1.f, 1.f, 1.f};
};

struct Skeleton
{
	std::vector<Bone> bones;
	std::unordered_map<std::string, int> boneNameToIndex;

	int findBone(const std::string &name) const;
};

struct AnimKeyframe
{
	float time = 0.f;
	glm::vec3 value3 = {0.f, 0.f, 0.f};
	glm::quat value4 = glm::quat(1.f, 0.f, 0.f, 0.f);
};

struct AnimChannel
{
	int boneIndex = -1;
	enum class Path { TRANSLATION, ROTATION, SCALE } path = Path::TRANSLATION;
	std::vector<AnimKeyframe> keyframes;
};

struct AnimClip
{
	std::string name;
	float duration = 0.f;
	bool looping = false;
	std::vector<AnimChannel> channels;
};

struct AnimPose
{
	glm::vec3 localPositions[MAX_BONES];
	glm::quat localRotations[MAX_BONES];
	glm::vec3 localScales[MAX_BONES];
};

struct AnimState
{
	int currentClip = -1;
	float time = 0.f;
	float speed = 1.f;

	int previousClip = -1;
	float previousTime = 0.f;
	float blendAlpha = 1.f;
	float blendDuration = 0.f;
	float blendTimer = 0.f;
};

void sampleClip(const AnimClip &clip, const Skeleton &skeleton, float time, AnimPose &outPose);
void blendPoses(const AnimPose &a, const AnimPose &b, float alpha, int boneCount, AnimPose &out);
void computeSkinMatrices(const Skeleton &skeleton, const AnimPose &pose, glm::mat4 *outMatrices);

void playClip(AnimState &state, int clipIndex, float blendTime = 0.1f);
void updateAnimState(AnimState &state, const std::vector<AnimClip> &clips, float dt);
void evaluateAnimState(const AnimState &state, const std::vector<AnimClip> &clips,
                       const Skeleton &skeleton, glm::mat4 *outSkinMatrices);
