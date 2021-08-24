// Class describing options, and namespace for global setting
#pragma once

#include <string>

#include "geometry.h"

class Options
{
public:
	size_t width = 800, height = 600;		// screen dimensions in pixels
	float bias = 0.0001f;	// bias is used to avoid self-intersections
	int maxRayDepth = 5;
	int nWorkers = 8;
	Vec3f backgroundColor { 0.0f, 0.0f, 0.0f };
	int acPenalty = 1;	// determines amount of acceleration structures
	char names[6][64];	// skybox names
	std::string rootPath = "D:\\dev\\CG\\RayTracing";
	std::string imageName = "out";
};


namespace options
{
	// global settings
	inline bool outputProgress			= 1;
	inline bool useBackfaceCulling		= 1;
	inline bool collectStatistics		= 0;
	inline bool enableOutput			= 1;
	inline bool imageOutput				= 1;
	inline bool useAC					= 1;
	inline bool showAC					= 0;
	inline bool useSkybox				= 1;
	inline bool useTextures				= 1;
	inline bool showNormals				= 0;
}
