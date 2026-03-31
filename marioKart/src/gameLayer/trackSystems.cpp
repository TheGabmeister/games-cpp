#include "trackSystems.h"
#include "gameConfig.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

float wrapDistance(float value, float length)
{
	if (length <= 0.f) { return 0.f; }

	float wrapped = std::fmod(value, length);
	if (wrapped < 0.f)
	{
		wrapped += length;
	}
	return wrapped;
}

bool crossedWrappedDistance(float previousValue, float currentValue, float threshold, float length)
{
	float previousWrapped = wrapDistance(previousValue, length);
	float currentWrapped = wrapDistance(currentValue, length);
	float wrappedThreshold = wrapDistance(threshold, length);

	if (currentValue - previousValue >= length)
	{
		return 1;
	}

	if (currentWrapped >= previousWrapped)
	{
		return previousWrapped < wrappedThreshold && currentWrapped >= wrappedThreshold;
	}

	return previousWrapped < wrappedThreshold || currentWrapped >= wrappedThreshold;
}

glm::vec3 sampleTrackPosition(const TrackState &track, float distanceAlongTrack)
{
	if (track.centerLine.size() < 2 || track.totalLength <= 0.f)
	{
		return {};
	}

	float remainingDistance = wrapDistance(distanceAlongTrack, track.totalLength);

	for (size_t i = 0; i < track.centerLine.size(); ++i)
	{
		size_t nextIndex = (i + 1) % track.centerLine.size();
		glm::vec3 start = track.centerLine[i];
		glm::vec3 end = track.centerLine[nextIndex];
		float segmentLength = glm::length(end - start);

		if (segmentLength <= 0.f)
		{
			continue;
		}

		if (remainingDistance <= segmentLength)
		{
			float factor = remainingDistance / segmentLength;
			return glm::mix(start, end, factor);
		}

		remainingDistance -= segmentLength;
	}

	return track.centerLine.front();
}

glm::vec3 sampleTrackForward(const TrackState &track, float distanceAlongTrack)
{
	if (track.centerLine.size() < 2 || track.totalLength <= 0.f)
	{
		return {1.f, 0.f, 0.f};
	}

	float remainingDistance = wrapDistance(distanceAlongTrack, track.totalLength);

	for (size_t i = 0; i < track.centerLine.size(); ++i)
	{
		size_t nextIndex = (i + 1) % track.centerLine.size();
		glm::vec3 start = track.centerLine[i];
		glm::vec3 end = track.centerLine[nextIndex];
		glm::vec3 delta = end - start;
		float segmentLength = glm::length(delta);

		if (segmentLength <= 0.f)
		{
			continue;
		}

		if (remainingDistance <= segmentLength)
		{
			return delta / segmentLength;
		}

		remainingDistance -= segmentLength;
	}

	glm::vec3 fallback = track.centerLine[1] - track.centerLine[0];
	float fallbackLength = glm::length(fallback);
	if (fallbackLength <= 0.f)
	{
		return {1.f, 0.f, 0.f};
	}

	return fallback / fallbackLength;
}

void buildTrack(TrackState &track)
{
	track.centerLine.clear();
	track.checkpoints.clear();

	// Oval track (stadium shape): clockwise loop when viewed from above
	// Bottom straight (heading +X)
	track.centerLine.push_back({-24.f, 0.f, -16.f});  // 0: start/finish
	track.centerLine.push_back({-10.f, 0.f, -16.f});  // 1
	track.centerLine.push_back({10.f, 0.f, -16.f});   // 2

	// Right turn
	track.centerLine.push_back({20.f, 0.f, -14.f});   // 3
	track.centerLine.push_back({26.f, 0.f, -8.f});    // 4
	track.centerLine.push_back({28.f, 0.f, 0.f});     // 5: right apex
	track.centerLine.push_back({26.f, 0.f, 8.f});     // 6
	track.centerLine.push_back({20.f, 0.f, 14.f});    // 7

	// Top straight (heading -X)
	track.centerLine.push_back({10.f, 0.f, 16.f});    // 8
	track.centerLine.push_back({-10.f, 0.f, 16.f});   // 9

	// Left turn
	track.centerLine.push_back({-20.f, 0.f, 14.f});   // 10
	track.centerLine.push_back({-26.f, 0.f, 8.f});    // 11
	track.centerLine.push_back({-28.f, 0.f, 0.f});    // 12: left apex
	track.centerLine.push_back({-26.f, 0.f, -8.f});   // 13
	track.centerLine.push_back({-20.f, 0.f, -14.f});  // 14

	track.roadHalfWidth = 6.f;
	track.wallHalfWidth = 10.f;

	// Compute total length
	track.totalLength = 0.f;
	for (size_t i = 0; i < track.centerLine.size(); ++i)
	{
		size_t next = (i + 1) % track.centerLine.size();
		track.totalLength += glm::length(track.centerLine[next] - track.centerLine[i]);
	}

	// Compute bounds from centerline + wall margin
	track.boundsMin = {FLT_MAX, FLT_MAX};
	track.boundsMax = {-FLT_MAX, -FLT_MAX};
	for (const auto &p : track.centerLine)
	{
		track.boundsMin = glm::min(track.boundsMin, glm::vec2(p.x, p.z));
		track.boundsMax = glm::max(track.boundsMax, glm::vec2(p.x, p.z));
	}
	track.boundsMin -= glm::vec2(track.wallHalfWidth + 5.f);
	track.boundsMax += glm::vec2(track.wallHalfWidth + 5.f);

	// Place checkpoints at specific centerline point indices
	auto addCheckpoint = [&](int pointIndex, glm::vec3 forward)
	{
		Checkpoint cp = {};
		glm::vec3 pos = track.centerLine[pointIndex];
		glm::vec3 right = glm::vec3(-forward.z, 0.f, forward.x);
		cp.start = pos - right * (track.roadHalfWidth + 4.f);
		cp.end = pos + right * (track.roadHalfWidth + 4.f);
		cp.forward = forward;

		float dist = 0.f;
		for (int i = 0; i < pointIndex; ++i)
		{
			dist += glm::length(track.centerLine[i + 1] - track.centerLine[i]);
		}
		cp.distanceAlongTrack = dist;

		track.checkpoints.push_back(cp);
	};

	addCheckpoint(0, {1.f, 0.f, 0.f});    // Start/finish
	addCheckpoint(5, {0.f, 0.f, 1.f});     // Right apex
	addCheckpoint(9, {-1.f, 0.f, 0.f});    // Top-left
	addCheckpoint(12, {0.f, 0.f, -1.f});   // Left apex

	// Boost pads
	track.boostPads.clear();
	track.boostPads.push_back({{0.f, 0.01f, -16.f}, {1.f, 0.f, 0.f}, 3.f, 1.5f});
	track.boostPads.push_back({{0.f, 0.01f, 16.f}, {-1.f, 0.f, 0.f}, 3.f, 1.5f});

	// Item boxes: placed at intervals along the track
	track.itemBoxes.clear();
	track.itemBoxes.push_back({{-16.f, 0.8f, -16.f}, 0.f, true});
	track.itemBoxes.push_back({{-16.f, 0.8f, -13.f}, 0.f, true});
	track.itemBoxes.push_back({{-16.f, 0.8f, -19.f}, 0.f, true});
	track.itemBoxes.push_back({{27.f, 0.8f, 0.f}, 0.f, true});
	track.itemBoxes.push_back({{-16.f, 0.8f, 16.f}, 0.f, true});
	track.itemBoxes.push_back({{-16.f, 0.8f, 13.f}, 0.f, true});
	track.itemBoxes.push_back({{-16.f, 0.8f, 19.f}, 0.f, true});
	track.itemBoxes.push_back({{-27.f, 0.8f, 0.f}, 0.f, true});
}

TrackQuery queryTrackPosition(glm::vec3 position, const TrackState &track)
{
	TrackQuery result = {};
	float bestDistSq = FLT_MAX;
	float runningDist = 0.f;

	for (size_t i = 0; i < track.centerLine.size(); ++i)
	{
		size_t next = (i + 1) % track.centerLine.size();
		glm::vec3 a = track.centerLine[i];
		glm::vec3 b = track.centerLine[next];
		glm::vec3 ab = b - a;
		float segLen = glm::length(ab);
		if (segLen <= 0.f) { runningDist += segLen; continue; }

		float t = glm::clamp(glm::dot(position - a, ab) / (segLen * segLen), 0.f, 1.f);
		glm::vec3 closest = a + ab * t;
		float distSq = glm::dot(position - closest, position - closest);

		if (distSq < bestDistSq)
		{
			bestDistSq = distSq;
			result.distanceAlongTrack = runningDist + t * segLen;
			result.closestPoint = closest;
		}

		runningDist += segLen;
	}

	glm::vec3 offset = position - result.closestPoint;
	offset.y = 0.f;
	result.lateralDistance = glm::length(offset);
	result.normal = (result.lateralDistance > 0.001f)
		? offset / result.lateralDistance
		: glm::vec3(0.f, 0.f, 1.f);

	return result;
}

void updateKartTransform(KartState &kart, const TrackState &track)
{
	glm::vec3 trackForward = sampleTrackForward(track, kart.distanceAlongTrack);
	glm::vec3 right = glm::normalize(glm::vec3{-trackForward.z, 0.f, trackForward.x});
	kart.position = sampleTrackPosition(track, kart.distanceAlongTrack) + right * kart.laneOffset;
	kart.velocity = trackForward * kart.speed;
	kart.heading = std::atan2(trackForward.z, trackForward.x);
	kart.lastSafePosition = kart.position;
	kart.lastSafeHeading = kart.heading;
}

float getSegmentProgress(const KartState &kart, const TrackState &track)
{
	if (track.checkpoints.empty() || track.totalLength <= 0.f)
	{
		return 0.f;
	}

	float localDistance = wrapDistance(kart.distanceAlongTrack, track.totalLength);
	float previousDistance = track.checkpoints[kart.progress.checkpointIndex].distanceAlongTrack;
	float nextDistance = track.totalLength;

	if (kart.progress.checkpointIndex + 1 < static_cast<int>(track.checkpoints.size()))
	{
		nextDistance = track.checkpoints[kart.progress.checkpointIndex + 1].distanceAlongTrack;
	}

	if (nextDistance < previousDistance)
	{
		nextDistance += track.totalLength;
	}

	if (localDistance < previousDistance)
	{
		localDistance += track.totalLength;
	}

	float segmentLength = nextDistance - previousDistance;
	if (segmentLength <= 0.f)
	{
		return 0.f;
	}

	return glm::clamp((localDistance - previousDistance) / segmentLength, 0.f, 1.f);
}
