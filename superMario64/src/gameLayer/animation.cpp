#define GLM_ENABLE_EXPERIMENTAL
#include "animation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

int Skeleton::findBone(const std::string &name) const
{
	auto it = boneNameToIndex.find(name);
	if (it != boneNameToIndex.end()) return it->second;
	return -1;
}

// ---- Keyframe Interpolation ----

static glm::vec3 lerpKeyframesVec3(const std::vector<AnimKeyframe> &keyframes, float time)
{
	if (keyframes.empty()) return {0.f, 0.f, 0.f};
	if (keyframes.size() == 1 || time <= keyframes.front().time) return keyframes.front().value3;
	if (time >= keyframes.back().time) return keyframes.back().value3;

	for (size_t i = 0; i < keyframes.size() - 1; i++)
	{
		if (time >= keyframes[i].time && time < keyframes[i + 1].time)
		{
			float segLen = keyframes[i + 1].time - keyframes[i].time;
			float t = (segLen > 0.0001f) ? (time - keyframes[i].time) / segLen : 0.f;
			return glm::mix(keyframes[i].value3, keyframes[i + 1].value3, t);
		}
	}
	return keyframes.back().value3;
}

static glm::quat lerpKeyframesQuat(const std::vector<AnimKeyframe> &keyframes, float time)
{
	if (keyframes.empty()) return glm::quat(1.f, 0.f, 0.f, 0.f);
	if (keyframes.size() == 1 || time <= keyframes.front().time) return keyframes.front().value4;
	if (time >= keyframes.back().time) return keyframes.back().value4;

	for (size_t i = 0; i < keyframes.size() - 1; i++)
	{
		if (time >= keyframes[i].time && time < keyframes[i + 1].time)
		{
			float segLen = keyframes[i + 1].time - keyframes[i].time;
			float t = (segLen > 0.0001f) ? (time - keyframes[i].time) / segLen : 0.f;
			return glm::slerp(keyframes[i].value4, keyframes[i + 1].value4, t);
		}
	}
	return keyframes.back().value4;
}

// ---- Sampling ----

void sampleClip(const AnimClip &clip, const Skeleton &skeleton, float time, AnimPose &outPose)
{
	int boneCount = (int)skeleton.bones.size();
	for (int i = 0; i < boneCount; i++)
	{
		outPose.localPositions[i] = skeleton.bones[i].localPosition;
		outPose.localRotations[i] = skeleton.bones[i].localRotation;
		outPose.localScales[i] = skeleton.bones[i].localScale;
	}

	for (const auto &ch : clip.channels)
	{
		if (ch.boneIndex < 0 || ch.boneIndex >= boneCount) continue;

		switch (ch.path)
		{
		case AnimChannel::Path::TRANSLATION:
			outPose.localPositions[ch.boneIndex] = lerpKeyframesVec3(ch.keyframes, time);
			break;
		case AnimChannel::Path::ROTATION:
			outPose.localRotations[ch.boneIndex] = lerpKeyframesQuat(ch.keyframes, time);
			break;
		case AnimChannel::Path::SCALE:
			outPose.localScales[ch.boneIndex] = lerpKeyframesVec3(ch.keyframes, time);
			break;
		}
	}
}

// ---- Blending ----

void blendPoses(const AnimPose &a, const AnimPose &b, float alpha, int boneCount, AnimPose &out)
{
	for (int i = 0; i < boneCount && i < MAX_BONES; i++)
	{
		out.localPositions[i] = glm::mix(a.localPositions[i], b.localPositions[i], alpha);
		out.localRotations[i] = glm::slerp(a.localRotations[i], b.localRotations[i], alpha);
		out.localScales[i] = glm::mix(a.localScales[i], b.localScales[i], alpha);
	}
}

// ---- Skin Matrices ----

void computeSkinMatrices(const Skeleton &skeleton, const AnimPose &pose, glm::mat4 *outMatrices)
{
	int boneCount = (int)skeleton.bones.size();
	std::vector<glm::mat4> globalTransforms(boneCount);

	for (int i = 0; i < boneCount; i++)
	{
		glm::mat4 T = glm::translate(glm::mat4(1.f), pose.localPositions[i]);
		glm::mat4 R = glm::toMat4(pose.localRotations[i]);
		glm::mat4 S = glm::scale(glm::mat4(1.f), pose.localScales[i]);
		glm::mat4 local = T * R * S;

		int parent = skeleton.bones[i].parentIndex;
		if (parent >= 0 && parent < i)
			globalTransforms[i] = globalTransforms[parent] * local;
		else
			globalTransforms[i] = local;

		outMatrices[i] = globalTransforms[i] * skeleton.bones[i].inverseBindMatrix;
	}
}

// ---- Playback Control ----

void playClip(AnimState &state, int clipIndex, float blendTime)
{
	if (state.currentClip == clipIndex)
	{
		state.time = 0.f;
		return;
	}

	state.previousClip = state.currentClip;
	state.previousTime = state.time;
	state.currentClip = clipIndex;
	state.time = 0.f;
	state.blendDuration = blendTime;
	state.blendTimer = 0.f;
	state.blendAlpha = (blendTime > 0.f) ? 0.f : 1.f;
}

void updateAnimState(AnimState &state, const std::vector<AnimClip> &clips, float dt)
{
	if (state.currentClip < 0 || state.currentClip >= (int)clips.size()) return;

	state.time += dt * state.speed;
	const auto &clip = clips[state.currentClip];

	if (clip.looping && clip.duration > 0.f)
		state.time = fmod(state.time, clip.duration);
	else if (state.time > clip.duration)
		state.time = clip.duration;

	if (state.blendAlpha < 1.f && state.blendDuration > 0.f)
	{
		state.blendTimer += dt;
		state.blendAlpha = std::min(1.f, state.blendTimer / state.blendDuration);
	}
}

void evaluateAnimState(const AnimState &state, const std::vector<AnimClip> &clips,
                       const Skeleton &skeleton, glm::mat4 *outSkinMatrices)
{
	int boneCount = (int)skeleton.bones.size();

	if (state.currentClip < 0 || state.currentClip >= (int)clips.size())
	{
		for (int i = 0; i < boneCount; i++)
			outSkinMatrices[i] = glm::mat4(1.f);
		return;
	}

	AnimPose currentPose;
	sampleClip(clips[state.currentClip], skeleton, state.time, currentPose);

	if (state.blendAlpha < 1.f && state.previousClip >= 0 &&
	    state.previousClip < (int)clips.size())
	{
		AnimPose prevPose;
		sampleClip(clips[state.previousClip], skeleton, state.previousTime, prevPose);

		AnimPose blendedPose;
		blendPoses(prevPose, currentPose, state.blendAlpha, boneCount, blendedPose);
		computeSkinMatrices(skeleton, blendedPose, outSkinMatrices);
	}
	else
	{
		computeSkinMatrices(skeleton, currentPose, outSkinMatrices);
	}
}
