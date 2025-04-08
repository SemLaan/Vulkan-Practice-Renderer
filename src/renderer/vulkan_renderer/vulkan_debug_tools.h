#pragma once
#include "defines.h"
#include "vulkan_types.h"



#ifndef GR_DIST

#define ADD_DEBUG_INSTANCE_EXTENSIONS(extensions, extensionCount) extensions[extensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME; extensionCount++
#define ADD_DEBUG_INSTANCE_LAYERS(layers, layerCount) layers[layerCount] = "VK_LAYER_KHRONOS_validation"; layerCount++

VkDebugUtilsMessengerCreateInfoEXT _GetDebugMessengerCreateInfo();

#define GetDebugMessengerCreateInfo(pNext_D)                                                                                                  \
        /* Enabling and disabling certain validation features */                                                                            \
        VkValidationFeatureEnableEXT enableSynchronizationValidationFeature = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;  \
                                                                                                                                            \
        VkValidationFeaturesEXT validationFeatures = {};                                                                                    \
        validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;                                                               \
        validationFeatures.pNext = nullptr;                                                                                                 \
        validationFeatures.enabledValidationFeatureCount = 1;                                                                               \
        validationFeatures.pEnabledValidationFeatures = &enableSynchronizationValidationFeature;                                            \
        validationFeatures.disabledValidationFeatureCount = 0;                                                                              \
        validationFeatures.pDisabledValidationFeatures = nullptr;                                                                           \
                                                                                                                                            \
        VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = _GetDebugMessengerCreateInfo();                                             \
        messengerCreateInfo.pNext = &validationFeatures;                                                                                    \
        pNext_D = &messengerCreateInfo; // Allows debug msg on instace creation and destruction

bool _CreateDebugMessenger();
void _DestroyDebugMessenger();

#define CreateDebugMessenger() _CreateDebugMessenger()
#define DestroyDebugMessenger() _DestroyDebugMessenger()

void _InsertDebugMemoryBarier(VkCommandBuffer commandBuffer);

#define INSERT_DEBUG_MEMORY_BARRIER(commandBuffer) _InsertDebugMemoryBarier(commandBuffer)

#else

#define ADD_DEBUG_INSTANCE_EXTENSIONS(extensions, extensionCount)
#define ADD_DEBUG_INSTANCE_LAYERS(layers, layerCount)

#define GetDebugMessengerCreateInfo(pNext) pNext = nullptr

#define CreateDebugMessenger()
#define DestroyDebugMessenger()

#define INSERT_DEBUG_MEMORY_BARRIER(commandBuffer) GRASSERT_MSG(false, "remove debug memory barrier")

#endif // !GR_DIST
