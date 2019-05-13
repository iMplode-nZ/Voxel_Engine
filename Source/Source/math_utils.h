#pragma once

#include "stdafx.h"

namespace Utils
{
	float lerp(float a, float b, float t);

	float lerp_t(float a, float b, float t);

	float align(float value, float size);

	void epsilon(float *val, float min, float max);

	float inverse_lerp(float a, float b, float t);

	float smoothstep(float edge0, float edge1, float x);

	float distPointToRect(glm::vec2 p, glm::vec2 rc, glm::vec2 rs);

	float distPointToPoint(glm::vec2 p1, glm::vec2 p2);

	float get_random(float low, float high);

	template<typename T>
	T max3(T first, T second, T third)
	{
		return std::max(first, std::max(second, third));
	}

	template<typename T>
	T min3(T first, T second, T third)
	{
		return std::min(first, std::min(second, third));
	}

	//DebugLineData PointsToASLine(const glm::vec2& p1, const glm::vec2& p2);
}