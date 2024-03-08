#pragma once

#include "defines.h"
#include "vulkan_types.h"


bool CheckRequiredExtensions(u32 requiredExtensionCount, const char** requiredExtensions, u32 availableExtensionCount, VkExtensionProperties* availableExtensions);

bool CheckRequiredLayers(u32 requiredLayerCount, const char** requiredLayers, u32 availableLayerCount, VkLayerProperties* availableLayers);

void TryDestroyResourcesPendingDestruction();
