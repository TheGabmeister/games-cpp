#pragma once

#include "gameState.h"
#include <glm/vec3.hpp>

struct TrackQuery
{
	float distanceAlongTrack = 0.f;
	float lateralDistance = 0.f;
	glm::vec3 closestPoint = {};
	glm::vec3 normal = {};
};

void buildTrack(TrackState &track);

float wrapDistance(float value, float length);
bool crossedWrappedDistance(float previousValue, float currentValue, float threshold, float length);

glm::vec3 sampleTrackPosition(const TrackState &track, float distanceAlongTrack);
glm::vec3 sampleTrackForward(const TrackState &track, float distanceAlongTrack);

TrackQuery queryTrackPosition(glm::vec3 position, const TrackState &track);

void updateKartTransform(KartState &kart, const TrackState &track);
float getSegmentProgress(const KartState &kart, const TrackState &track);
