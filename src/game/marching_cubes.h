#pragma once
#include "renderer/material.h"


/// @brief Generates 3d floating point density array that defines the terrain.
void MCGenerateDensityMap();

/// @brief Generates a mesh for the generated density map. Density map is assumed to be generated already
void MCGenerateMesh();

/// @brief Renders the mesh, assumes that the mesh has been generated.
void MCRenderWorld();

/// @brief Cleans up all marching cubes resources.
void MCDestroyMeshAndDensityMap();
