#include "../renderer.h"

#include "containers/darray.h"
#include "core/asserts.h"
#include "core/event.h"
#include "core/logger.h"
#include "core/meminc.h"
#include "math/lin_alg.h"
#include "renderer/obj_loader.h"
#include "core/platform.h"

#include "renderer/texture.h"
#include "vulkan_buffer.h"
#include "vulkan_command_buffer.h"
#include "vulkan_debug_tools.h"
#include "vulkan_image.h"
#include "vulkan_platform.h"
#include "vulkan_swapchain.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"
#include "vulkan_shader.h"
#include "vulkan_memory.h"
#include "vulkan_transfer.h"

#define RENDERER_ALLOCATOR_SIZE (50 * MiB)
#define RENDERER_POOL_ALLOCATOR_32b_SIZE 200
#define RENDERER_RESOURCE_ACQUISITION_SIZE 200
#define MAX_VERTEX_BUFFERS_PER_DRAW_CALL 2

DEFINE_DARRAY_TYPE(VkExtensionProperties);
DEFINE_DARRAY_TYPE(VkLayerProperties);
DEFINE_DARRAY_TYPE(u32);

RendererState* vk_state = nullptr;

static bool OnWindowResize(EventCode type, EventData data);

bool InitializeRenderer(RendererInitSettings settings)
{
	GRASSERT_DEBUG(vk_state == nullptr); // If this triggers init got called twice
	_INFO("Initializing renderer subsystem...");

	vk_state = AlignedAlloc(GetGlobalAllocator(), sizeof(RendererState), 64 /*cache line*/);
	MemoryZero(vk_state, sizeof(*vk_state));
	CreateFreelistAllocator("renderer allocator", GetGlobalAllocator(), RENDERER_ALLOCATOR_SIZE, &vk_state->rendererAllocator, true);

	vk_state->vkAllocator = nullptr; // TODO: add something that tracks vulkan API allocations in debug mode

	vk_state->currentFrameIndex = 0;
	vk_state->currentInFlightFrameIndex = 0;
	vk_state->shouldRecreateSwapchain = false;
	vk_state->requestedPresentMode = settings.presentMode;
	vk_state->mipGenerationQueue = VulkanImageRefDarrayCreate(10, vk_state->rendererAllocator);

	RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

	// ================== Getting required instance extensions and layers ================================
#define MAX_INSTANCE_EXTENSIONS 10
#define MAX_INSTANCE_LAYERS 10
	// Getting required extensions
	const char* requiredInstanceExtensions[MAX_INSTANCE_EXTENSIONS];
	u32 requiredInstanceExtensionCount = 0;

	requiredInstanceExtensions[requiredInstanceExtensionCount] = VK_KHR_SURFACE_EXTENSION_NAME;
	requiredInstanceExtensionCount++;
	GetPlatformExtensions(&requiredInstanceExtensionCount, requiredInstanceExtensions);
	ADD_DEBUG_INSTANCE_EXTENSIONS(requiredInstanceExtensions, requiredInstanceExtensionCount);

	// Getting required layers
	const char* requiredInstanceLayers[MAX_INSTANCE_LAYERS];
	u32 requiredInstanceLayerCount = 0;
	ADD_DEBUG_INSTANCE_LAYERS(requiredInstanceLayers, requiredInstanceLayerCount);

	// ============================================================================================================================================================
	// =========================================================== Creating instance ==============================================================================
	// ============================================================================================================================================================
	{
		// ================ App info =============================================
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = "Test app";
		appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
		appInfo.pEngineName = "Goril";
		appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		{
			// Checking if required extensions are available
			u32 availableExtensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
			VkExtensionPropertiesDarray* availableExtensionsDarray = VkExtensionPropertiesDarrayCreateWithSize(availableExtensionCount, vk_state->rendererAllocator);
			vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensionsDarray->data);

			bool extensionsAvailable = CheckRequiredExtensions(requiredInstanceExtensionCount, requiredInstanceExtensions, availableExtensionCount, availableExtensionsDarray->data);

			DarrayDestroy(availableExtensionsDarray);

			if (!extensionsAvailable)
			{
				_FATAL("Couldn't find required Vulkan extensions");
				return false;
			}
			else
				_TRACE("Required Vulkan extensions found");
		}

		{
			// Checking if required layers are available
			u32 availableLayerCount = 0;
			vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
			VkLayerPropertiesDarray* availableLayersDarray = VkLayerPropertiesDarrayCreate(availableLayerCount, vk_state->rendererAllocator);
			vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayersDarray->data);

			bool layersAvailable = CheckRequiredLayers(requiredInstanceLayerCount, requiredInstanceLayers, availableLayerCount, availableLayersDarray->data);

			DarrayDestroy(availableLayersDarray);

			if (!layersAvailable)
			{
				_FATAL("Couldn't find required Vulkan layers");
				return false;
			}
			else
				_TRACE("Required Vulkan layers found");
		}

		// ================== Creating instance =================================
		{
			VkInstanceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

			GetDebugMessengerCreateInfo(createInfo.pNext);
			createInfo.flags = 0;
			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledLayerCount = requiredInstanceLayerCount;
			createInfo.ppEnabledLayerNames = requiredInstanceLayers;
			createInfo.enabledExtensionCount = requiredInstanceExtensionCount;
			createInfo.ppEnabledExtensionNames = requiredInstanceExtensions;

			VkResult result = vkCreateInstance(&createInfo, vk_state->vkAllocator, &vk_state->instance);

			if (result != VK_SUCCESS)
			{
				_FATAL("Failed to create Vulkan instance");
				return false;
			}
		}

		_TRACE("Vulkan instance created");
	}

	// ============================================================================================================================================================
	// =============== Creating debug messenger ===================================================================================================================
	// ============================================================================================================================================================
	CreateDebugMessenger();

	// ============================================================================================================================================================
	// ================ Creating a surface ========================================================================================================================
	// ============================================================================================================================================================
	if (!PlatformCreateSurface(vk_state->instance, vk_state->vkAllocator, &vk_state->surface))
	{
		_FATAL("Failed to create Vulkan surface");
		return false;
	}

	// ============================================================================================================================================================
	// ================ Getting a physical device =================================================================================================================
	// ============================================================================================================================================================
#define MAX_DEVICE_EXTENSIONS 10
	const char* requiredDeviceExtensions[MAX_DEVICE_EXTENSIONS];
	u32 requiredDeviceExtensionCount = 0;

	requiredDeviceExtensions[requiredDeviceExtensionCount + 0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	requiredDeviceExtensionCount += 1;

	{
		vk_state->physicalDevice = VK_NULL_HANDLE;

		u32 deviceCount = 0;
		VK_CHECK(vkEnumeratePhysicalDevices(vk_state->instance, &deviceCount, nullptr));
		if (deviceCount == 0)
		{
			_FATAL("No Vulkan devices found");
			return false;
		}

		VkPhysicalDevice* availableDevices = Alloc(vk_state->rendererAllocator, sizeof(*availableDevices) * deviceCount);
		VK_CHECK(vkEnumeratePhysicalDevices(vk_state->instance, &deviceCount, availableDevices));

		/// TODO: better device selection
		for (u32 i = 0; i < deviceCount; ++i)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(availableDevices[i], &properties);
			bool isDiscrete = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
			bool extensionsAvailable;

			{ // Checking if the device has the required extensions
				u32 availableExtensionCount = 0;
				VK_CHECK(vkEnumerateDeviceExtensionProperties(availableDevices[i], nullptr, &availableExtensionCount, nullptr));
				VkExtensionPropertiesDarray* availableExtensionsDarray = VkExtensionPropertiesDarrayCreateWithSize(availableExtensionCount, GetGlobalAllocator()); // TODO: change from darray to just array
				VK_CHECK(vkEnumerateDeviceExtensionProperties(availableDevices[i], nullptr, &availableExtensionCount, availableExtensionsDarray->data));

				extensionsAvailable = CheckRequiredExtensions(requiredDeviceExtensionCount, requiredDeviceExtensions, availableExtensionCount, availableExtensionsDarray->data);

				DarrayDestroy(availableExtensionsDarray);
			}

			if (isDiscrete && extensionsAvailable)
			{
				_TRACE("Device with required extensions, features and properties found");
				SwapchainSupportDetails swapchainSupport = QuerySwapchainSupport(availableDevices[i], vk_state->surface);
				if (swapchainSupport.formatCount != 0 && swapchainSupport.presentModeCount != 0)
				{
					vk_state->physicalDevice = availableDevices[i];
					vk_state->swapchainSupport = swapchainSupport;
					vk_state->deviceProperties = properties;
					break;
				}
				Free(vk_state->rendererAllocator, swapchainSupport.formats);
				Free(vk_state->rendererAllocator, swapchainSupport.presentModes);
			}
		}

		Free(vk_state->rendererAllocator, availableDevices);

		if (vk_state->physicalDevice == VK_NULL_HANDLE)
		{
			_FATAL("No suitable devices found");
			return false;
		}

		_TRACE("Successfully selected physical vulkan device");
	}

	// ============================================================================================================================================================
	// ================== Getting device queue families ===========================================================================================================
	// ============================================================================================================================================================
	{
		u32 queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(vk_state->physicalDevice, &queueFamilyCount, nullptr);
		VkQueueFamilyProperties* availableQueueFamilies = Alloc(vk_state->rendererAllocator, sizeof(*availableQueueFamilies) * queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(vk_state->physicalDevice, &queueFamilyCount, availableQueueFamilies);

		vk_state->transferQueue.index = UINT32_MAX;

		for (u32 i = 0; i < queueFamilyCount; ++i)
		{
			VkBool32 presentSupport = false;
			VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vk_state->physicalDevice, i, vk_state->surface, &presentSupport));
			bool graphicsSupport = availableQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
			bool transferSupport = availableQueueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT;
			if (graphicsSupport)
				vk_state->graphicsQueue.index = i;
			if (presentSupport)
				vk_state->presentQueue.index = i;
			if (transferSupport && !graphicsSupport)
				vk_state->transferQueue.index = i;
		}

		if (vk_state->transferQueue.index == UINT32_MAX)
			vk_state->transferQueue.index = vk_state->graphicsQueue.index;
		/// TODO: check if the device even has queue families for all these things, if not fail startup (is this even required? i think implementations need at least transfer and graphics(?), and compute and present are implied by the existence of the extensions)
		Free(vk_state->rendererAllocator, availableQueueFamilies);
	}

	// ============================================================================================================================================================
	// ===================== Creating logical device ==============================================================================================================
	// ============================================================================================================================================================
	{
		// ===================== Specifying queues for logical device =================================
		u32Darray* uniqueQueueFamiliesDarray = u32DarrayCreate(5, vk_state->rendererAllocator);
		if (!DarrayContains(uniqueQueueFamiliesDarray, &vk_state->graphicsQueue.index))
			u32DarrayPushback(uniqueQueueFamiliesDarray, &vk_state->graphicsQueue.index);
		if (!DarrayContains(uniqueQueueFamiliesDarray, &vk_state->presentQueue.index))
			u32DarrayPushback(uniqueQueueFamiliesDarray, &vk_state->presentQueue.index);
		if (!DarrayContains(uniqueQueueFamiliesDarray, &vk_state->transferQueue.index))
			u32DarrayPushback(uniqueQueueFamiliesDarray, &vk_state->transferQueue.index);

		const f32 queuePriority = 1.0f;

		u32 uniqueQueueCount = uniqueQueueFamiliesDarray->size;

		VkDeviceQueueCreateInfo* queueCreateInfos = Alloc(vk_state->rendererAllocator, sizeof(*queueCreateInfos) * uniqueQueueCount);

		for (u32 i = 0; i < uniqueQueueCount; ++i)
		{
			queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfos[i].pNext = nullptr;
			queueCreateInfos[i].flags = 0;
			queueCreateInfos[i].queueFamilyIndex = uniqueQueueFamiliesDarray->data[i];
			queueCreateInfos[i].queueCount = 1;
			queueCreateInfos[i].pQueuePriorities = &queuePriority;
		}

		DarrayDestroy(uniqueQueueFamiliesDarray);

		// ===================== Specifying features for logical device ==============================
		VkPhysicalDeviceFeatures deviceFeatures = {};
		/// TODO: add required device features here, these should be retrieved from the application config

		/// Put new extension features above here and make the extension feature under this point to that new feature
		VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {};
		dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		dynamicRenderingFeatures.pNext = nullptr;
		dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

		VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures = {};
		timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
		timelineSemaphoreFeatures.pNext = &dynamicRenderingFeatures;
		timelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;

		VkPhysicalDeviceSynchronization2Features synchronization2Features = {};
		synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
		synchronization2Features.pNext = &timelineSemaphoreFeatures;
		synchronization2Features.synchronization2 = VK_TRUE;

		VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures2.pNext = &synchronization2Features;
		deviceFeatures2.features = deviceFeatures;

		// ===================== Creating logical device =============================================
		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext = &deviceFeatures2;
		createInfo.flags = 0;
		createInfo.queueCreateInfoCount = uniqueQueueCount;
		createInfo.pQueueCreateInfos = queueCreateInfos;
		createInfo.enabledLayerCount = requiredInstanceLayerCount;
		createInfo.ppEnabledLayerNames = requiredInstanceLayers;
		createInfo.enabledExtensionCount = requiredDeviceExtensionCount;
		createInfo.ppEnabledExtensionNames = requiredDeviceExtensions;
		createInfo.pEnabledFeatures = nullptr;

		VK_CHECK(vkCreateDevice(vk_state->physicalDevice, &createInfo, vk_state->vkAllocator, &vk_state->device));

		Free(vk_state->rendererAllocator, queueCreateInfos);

		_TRACE("Successfully created vulkan logical device");
	}

	// ============================================================================================================================================================
	// ===================== sets up queues and command pools =====================================================================================================
	// ============================================================================================================================================================
	{
		// =================== Getting the device queues ======================================================		
		/// TODO: get compute queue
		// Present, Graphics, transfer and (in the future) compute queue
		vkGetDeviceQueue(vk_state->device, vk_state->presentQueue.index, 0, &vk_state->presentQueue.handle);
		vkGetDeviceQueue(vk_state->device, vk_state->graphicsQueue.index, 0, &vk_state->graphicsQueue.handle);
		vkGetDeviceQueue(vk_state->device, vk_state->transferQueue.index, 0, &vk_state->transferQueue.handle);

		InitDeferredResourceDestructionState(&vk_state->deferredResourceDestruction, 200/*200 objects can be destroyed per frame without having to do dynamic allocation*/);

		// ==================== Creating command pools for each of the queue families =============================
		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.pNext = nullptr;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = vk_state->graphicsQueue.index;

		VK_CHECK(vkCreateCommandPool(vk_state->device, &commandPoolCreateInfo, vk_state->vkAllocator, &vk_state->graphicsQueue.commandPool));

		commandPoolCreateInfo.queueFamilyIndex = vk_state->transferQueue.index;

		VK_CHECK(vkCreateCommandPool(vk_state->device, &commandPoolCreateInfo, vk_state->vkAllocator, &vk_state->transferQueue.commandPool));

		commandPoolCreateInfo.queueFamilyIndex = vk_state->presentQueue.index;

		VK_CHECK(vkCreateCommandPool(vk_state->device, &commandPoolCreateInfo, vk_state->vkAllocator, &vk_state->presentQueue.commandPool));

		/// TODO: create compute command pool

		_TRACE("Successfully retrieved vulkan queues and created command pools");
	}

	// ============================================================================================================================================================
	// ============================ Allocate graphics command buffers =============================================================================================
	// ============================================================================================================================================================
	for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		AllocateCommandBuffer(&vk_state->graphicsQueue, &vk_state->graphicsCommandBuffers[i]);
		AllocateCommandBuffer(&vk_state->presentQueue, &vk_state->presentCommandBuffers[i]);
	}

	// ============================================================================================================================================================
	// ================================ Create sync objects =======================================================================================================
	// ============================================================================================================================================================
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			VK_CHECK(vkCreateSemaphore(vk_state->device, &semaphoreCreateInfo, vk_state->vkAllocator, &vk_state->imageAvailableSemaphores[i]));
			VK_CHECK(vkCreateSemaphore(vk_state->device, &semaphoreCreateInfo, vk_state->vkAllocator, &vk_state->prePresentCompleteSemaphores[i]));
		}

		VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {};
		semaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		semaphoreTypeInfo.pNext = 0;
		semaphoreTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		semaphoreTypeInfo.initialValue = 0;

		VkSemaphoreCreateInfo timelineSemaphoreCreateInfo = {};
		timelineSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		timelineSemaphoreCreateInfo.pNext = &semaphoreTypeInfo;
		timelineSemaphoreCreateInfo.flags = 0;

		// max max frames in flight just needs to be higher than any sensible maxFramesInFlight value,
		// look at the wait for semaphores function at the start of the renderloop to understand why
		vk_state->frameSemaphore.submitValue = MAX_FRAMES_IN_FLIGHT;
		vk_state->duplicatePrePresentCompleteSemaphore.submitValue = MAX_FRAMES_IN_FLIGHT;
		semaphoreTypeInfo.initialValue = MAX_FRAMES_IN_FLIGHT;

		VK_CHECK(vkCreateSemaphore(vk_state->device, &timelineSemaphoreCreateInfo, vk_state->vkAllocator, &vk_state->frameSemaphore.handle));
		VK_CHECK(vkCreateSemaphore(vk_state->device, &timelineSemaphoreCreateInfo, vk_state->vkAllocator, &vk_state->duplicatePrePresentCompleteSemaphore.handle));

		_TRACE("Vulkan sync objects created successfully");
	}

	// ============================================================================================================================================================
	// ======================== Initializing the vulkan memory system ============================================================================================================
	// ============================================================================================================================================================
	VulkanMemoryInit();

	// ============================================================================================================================================================
	// ======================== Initializing the vulkan transfer system ============================================================================================================
	// ============================================================================================================================================================
	VulkanTransferInit();

	// ============================================================================================================================================================
	// ======================== Finding render target formats ============================================================================================================
	// ============================================================================================================================================================
	{
		// Checking color format support
		{
			VkFormatProperties2 formatProperties = {};
			formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
			VkFormat requiredColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
			vkGetPhysicalDeviceFormatProperties2(vk_state->physicalDevice, requiredColorFormat, &formatProperties);

			VkFormatFeatureFlags requiredFormatFeatures = VK_FORMAT_FEATURE_2_BLIT_SRC_BIT | VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT;
			GRASSERT_MSG((formatProperties.formatProperties.optimalTilingFeatures & requiredFormatFeatures) == requiredFormatFeatures, "Color format required for render target not supported");

			vk_state->renderTargetColorFormat = requiredColorFormat;
		}

		// Checking depth format support
		{
			VkFormatProperties2 formatProperties = {};
			formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
			VkFormat preferredDepthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
			VkFormat fallbackDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
			vkGetPhysicalDeviceFormatProperties2(vk_state->physicalDevice, preferredDepthFormat, &formatProperties);

			VkFormatFeatureFlags requiredFormatFeatures = VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT;
			bool d32s8_support = requiredFormatFeatures == (formatProperties.formatProperties.optimalTilingFeatures & requiredFormatFeatures);

			_INFO("Chosen depth format: %s", d32s8_support ? "D32S8" : "D24S8");

			vk_state->renderTargetDepthFormat = d32s8_support ? preferredDepthFormat : fallbackDepthFormat;
		}
	}

	// ============================================================================================================================================================
	// ======================== Creating the swapchain ============================================================================================================
	// ============================================================================================================================================================
	CreateSwapchain(settings.presentMode);

	// ============================================================================================================================================================
	// ======================== Creating the descriptor pool ============================================================================================================
	// ============================================================================================================================================================
	VkDescriptorPoolSize descriptorPoolSizes[2] = {};
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[0].descriptorCount = 200;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSizes[1].descriptorCount = 200;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 200;
	descriptorPoolCreateInfo.poolSizeCount = 2;
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;

	VK_CHECK(vkCreateDescriptorPool(vk_state->device, &descriptorPoolCreateInfo, vk_state->vkAllocator, &vk_state->descriptorPool));

	// ============================================================================================================================================================
	// ======================== Creating samplers ============================================================================================================
	// ============================================================================================================================================================
	vk_state->samplers = Alloc(vk_state->rendererAllocator, sizeof(*vk_state->samplers));

	{
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.pNext = nullptr;
		samplerCreateInfo.flags = 0;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

		VK_CHECK(vkCreateSampler(vk_state->device, &samplerCreateInfo, vk_state->vkAllocator, &vk_state->samplers->nearestClampEdge));

		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		VK_CHECK(vkCreateSampler(vk_state->device, &samplerCreateInfo, vk_state->vkAllocator, &vk_state->samplers->nearestRepeat));

		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		VK_CHECK(vkCreateSampler(vk_state->device, &samplerCreateInfo, vk_state->vkAllocator, &vk_state->samplers->linearRepeat));

		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		VK_CHECK(vkCreateSampler(vk_state->device, &samplerCreateInfo, vk_state->vkAllocator, &vk_state->samplers->linearClampEdge));

		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCreateInfo.compareEnable = VK_TRUE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_LESS;

		VK_CHECK(vkCreateSampler(vk_state->device, &samplerCreateInfo, vk_state->vkAllocator, &vk_state->samplers->shadow));
	}

	// ============================================================================================================================================================
	// ======================== Creating GlobalUBO descriptor set and buffer ============================================================================================================
	// ============================================================================================================================================================
	// Descriptor set layout
	VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[1] = {};
	descriptorSetLayoutBindings[0].binding = 0;
	descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBindings[0].descriptorCount = 1;
	descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	descriptorSetLayoutBindings[0].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;

	VK_CHECK(vkCreateDescriptorSetLayout(vk_state->device, &descriptorSetLayoutCreateInfo, vk_state->vkAllocator, &vk_state->globalDescriptorSetLayout));

	// Creating backing buffer and memory and mapping the memory
	VkDeviceSize uniformBufferSize = sizeof(GlobalUniformObject);

	vk_state->globalUniformBufferArray = Alloc(vk_state->rendererAllocator, MAX_FRAMES_IN_FLIGHT * sizeof(*vk_state->globalUniformBufferArray));
	vk_state->globalUniformAllocationArray = Alloc(vk_state->rendererAllocator, MAX_FRAMES_IN_FLIGHT * sizeof(*vk_state->globalUniformAllocationArray));
	for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		BufferCreate(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemType(MEMORY_TYPE_DYNAMIC), &vk_state->globalUniformBufferArray[i], &vk_state->globalUniformAllocationArray[i]);
	}

	// Allocating descriptor sets
	VkDescriptorSetLayout descriptorSetLayouts[MAX_FRAMES_IN_FLIGHT];
	for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		descriptorSetLayouts[i] = vk_state->globalDescriptorSetLayout;
	}

	VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
	descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocInfo.pNext = nullptr;
	descriptorSetAllocInfo.descriptorPool = vk_state->descriptorPool;
	descriptorSetAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	descriptorSetAllocInfo.pSetLayouts = descriptorSetLayouts;

	vk_state->globalDescriptorSetArray = Alloc(vk_state->rendererAllocator, MAX_FRAMES_IN_FLIGHT * sizeof(*vk_state->globalDescriptorSetArray));

	VK_CHECK(vkAllocateDescriptorSets(vk_state->device, &descriptorSetAllocInfo, vk_state->globalDescriptorSetArray));

	// Updating descriptor sets to link them to the backing buffer
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = vk_state->globalUniformBufferArray[i];
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(GlobalUniformObject);

		VkWriteDescriptorSet descriptorWrites[1] = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].pNext = nullptr;
		descriptorWrites[0].dstSet = vk_state->globalDescriptorSetArray[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].pImageInfo = nullptr;
		descriptorWrites[0].pBufferInfo = &descriptorBufferInfo;
		descriptorWrites[0].pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(vk_state->device, 1, descriptorWrites, 0, nullptr);
	}

	// ============================================================================================================================================================
	// ============================ Creating default texture ======================================================================================================
	// ============================================================================================================================================================
	{
#define DEFAULT_TEXTURE_SIZE 256

		const u32 defaultTexturePixelCount = DEFAULT_TEXTURE_SIZE * DEFAULT_TEXTURE_SIZE;
		u8 defaultTexturePixels[DEFAULT_TEXTURE_SIZE * DEFAULT_TEXTURE_SIZE * TEXTURE_CHANNELS] = {};

		const u32 halfOfTexturePixels = defaultTexturePixelCount / 2;

		for (u32 i = 0; i < defaultTexturePixelCount * TEXTURE_CHANNELS; i += TEXTURE_CHANNELS)
		{
			u32 pixelIndex = i / TEXTURE_CHANNELS;

			if ((pixelIndex < halfOfTexturePixels && (pixelIndex % DEFAULT_TEXTURE_SIZE) < (DEFAULT_TEXTURE_SIZE / 2)) ||
				(pixelIndex >= halfOfTexturePixels && (pixelIndex % DEFAULT_TEXTURE_SIZE) >= (DEFAULT_TEXTURE_SIZE / 2)))
			{
				defaultTexturePixels[i + 0] = 150;
				defaultTexturePixels[i + 1] = 50;
				defaultTexturePixels[i + 2] = 200;
				defaultTexturePixels[i + 3] = 255;
			}
			else
			{
				defaultTexturePixels[i + 0] = 0;
				defaultTexturePixels[i + 1] = 0;
				defaultTexturePixels[i + 2] = 0;
				defaultTexturePixels[i + 3] = 255;
			}
		}

		vk_state->defaultTexture = TextureCreate(DEFAULT_TEXTURE_SIZE, DEFAULT_TEXTURE_SIZE, defaultTexturePixels, TEXTURE_STORAGE_RGBA8SRGB, false);
	}

	// ============================================================================================================================================================
	// ============================ Creating shader map and default shader and material ======================================================================================================
	// ============================================================================================================================================================
	vk_state->shaderMap = SimpleMapCreate(vk_state->rendererAllocator, MAX_SHADERS);

	ShaderCreateInfo shaderCreateInfo = {};
	shaderCreateInfo.vertexShaderName = DEFAULT_SHADER_NAME;
	shaderCreateInfo.fragmentShaderName = DEFAULT_SHADER_NAME;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2;
	shaderCreateInfo.renderTargetColor = true;
	shaderCreateInfo.renderTargetDepth = true;
	shaderCreateInfo.renderTargetStencil = false;
	ShaderCreate(DEFAULT_SHADER_NAME, &shaderCreateInfo);
	vk_state->defaultMaterial = MaterialCreate(ShaderGetRef(DEFAULT_SHADER_NAME));
	vec4 defaultColor = vec4_create(1, 0.5f, 1, 1);
	MaterialUpdateProperty(vk_state->defaultMaterial, "color", &defaultColor);

	// ============================================================================================================================================================
	// ============================ Loading basic meshes ======================================================================================================
	// ============================================================================================================================================================
	vk_state->basicMeshMap = SimpleMapCreate(vk_state->rendererAllocator, BASIC_MESH_COUNT + 20/*making sure collisions don't occur*/);

	u32 currentBasicMeshIndex = 0;

	GPUMesh* basicMeshDataArray = Alloc(vk_state->rendererAllocator, sizeof(*basicMeshDataArray) * BASIC_MESH_COUNT);
	LoadObj("models/quad.obj", &basicMeshDataArray[currentBasicMeshIndex].vertexBuffer, &basicMeshDataArray[currentBasicMeshIndex].indexBuffer, false);
	SimpleMapInsert(vk_state->basicMeshMap, BASIC_MESH_NAME_QUAD, basicMeshDataArray + currentBasicMeshIndex);
	currentBasicMeshIndex++;

	LoadObj("models/sphere.obj", &basicMeshDataArray[currentBasicMeshIndex].vertexBuffer, &basicMeshDataArray[currentBasicMeshIndex].indexBuffer, false);
	SimpleMapInsert(vk_state->basicMeshMap, BASIC_MESH_NAME_SPHERE, basicMeshDataArray + currentBasicMeshIndex);
	currentBasicMeshIndex++;

	LoadObj("models/cube.obj", &basicMeshDataArray[currentBasicMeshIndex].vertexBuffer, &basicMeshDataArray[currentBasicMeshIndex].indexBuffer, false);
	SimpleMapInsert(vk_state->basicMeshMap, BASIC_MESH_NAME_CUBE, basicMeshDataArray + currentBasicMeshIndex);
	currentBasicMeshIndex++;

#define FULLSCREEN_TRIANGLE_VERT_COUNT 3
#define FULLSCREEN_TRIANGLE_VERT_FLOAT_COUNT 5
	f32 fullscreenTriangleVertices[FULLSCREEN_TRIANGLE_VERT_COUNT * FULLSCREEN_TRIANGLE_VERT_FLOAT_COUNT] =
	{
		//Clip Pos       
		-1, 3, 0.f, 		0, 2,
		3, -1, 0.f, 		2, 0,
		-1, -1, 0.f, 		0, 0,
	};
	u32 fullscreenTriangleIndices[FULLSCREEN_TRIANGLE_VERT_COUNT] = { 0, 1, 2 };
	basicMeshDataArray[currentBasicMeshIndex].vertexBuffer = VertexBufferCreate(fullscreenTriangleVertices, sizeof(fullscreenTriangleVertices));
	basicMeshDataArray[currentBasicMeshIndex].indexBuffer = IndexBufferCreate(fullscreenTriangleIndices, FULLSCREEN_TRIANGLE_VERT_COUNT);
	SimpleMapInsert(vk_state->basicMeshMap, BASIC_MESH_NAME_FULL_SCREEN_TRIANGLE, basicMeshDataArray + currentBasicMeshIndex);
	currentBasicMeshIndex++;

	GRASSERT_DEBUG(currentBasicMeshIndex == BASIC_MESH_COUNT);

	return true;
}

void WaitForGPUIdle()
{
	vkDeviceWaitIdle(vk_state->device);
}

void ShutdownRenderer()
{
	if (vk_state == nullptr)
	{
		_INFO("Renderer startup failed, skipping shutdown");
		return;
	}
	else
	{
		_INFO("Shutting down renderer subsystem...");
	}

	UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

	// Idling before destroying resources
	if (vk_state->device)
		VK_CHECK(vkDeviceWaitIdle(vk_state->device));

	VkSemaphoreSignalInfo signalInfo;
	signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	signalInfo.pNext = NULL;
	signalInfo.semaphore = vk_state->frameSemaphore.handle;
	signalInfo.value = vk_state->frameSemaphore.submitValue + 1;

	vkSignalSemaphore(vk_state->device, &signalInfo);

	// ============================================================================================================================================================
	// ============================ Destroying basic meshes ======================================================================================================
	// ============================================================================================================================================================
	// See the creation of the basic meshes to understand why this works
	GPUMesh* basicMeshDataArray = SimpleMapLookup(vk_state->basicMeshMap, BASIC_MESH_NAME_QUAD);

	for (int i = 0; i < BASIC_MESH_COUNT; i++)
	{
		VertexBufferDestroy(basicMeshDataArray[i].vertexBuffer);
		IndexBufferDestroy(basicMeshDataArray[i].indexBuffer);
	}

	SimpleMapDestroy(vk_state->basicMeshMap);
	Free(vk_state->rendererAllocator, basicMeshDataArray);

	// ============================================================================================================================================================
	// ============================ Destroying material, left over shaders and shader map ======================================================================================================
	// ============================================================================================================================================================
	MaterialDestroy(vk_state->defaultMaterial);

	u32 shaderArraySize;
	void** shaderArray = SimpleMapGetBackingArrayRef(vk_state->shaderMap, &shaderArraySize);

	for (int i = 0; i < shaderArraySize; i++)
	{
		VulkanShader* shader = shaderArray[i];
		if (shader)
			ShaderDestroyInternal(shader);
	}

	SimpleMapDestroy(vk_state->shaderMap);

	// ============================================================================================================================================================
	// ============================ Destroying default texture ======================================================================================================
	// ============================================================================================================================================================
	if (vk_state->defaultTexture.internalState)
		TextureDestroy(vk_state->defaultTexture);

	TryDestroyResourcesPendingDestruction();

	// ============================================================================================================================================================
	// ============================ Destroying global ubo stuff (arrays, buffers, memory, descriptor set layout, descriptor sets) =================================
	// ============================================================================================================================================================
	if (vk_state->globalDescriptorSetLayout)
	{
		vkDestroyDescriptorSetLayout(vk_state->device, vk_state->globalDescriptorSetLayout, vk_state->vkAllocator);
	}

	if (vk_state->globalDescriptorSetArray)
	{
		Free(vk_state->rendererAllocator, vk_state->globalDescriptorSetArray);
	}

	for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		BufferDestroy(&vk_state->globalUniformBufferArray[i], &vk_state->globalUniformAllocationArray[i]);
	}

	Free(vk_state->rendererAllocator, vk_state->globalUniformBufferArray);
	Free(vk_state->rendererAllocator, vk_state->globalUniformAllocationArray);

	// ============================================================================================================================================================
	// ====================== Destroying samplers if they were created ==============================================================================================
	// ============================================================================================================================================================
	if (vk_state->samplers)
	{
		vkDestroySampler(vk_state->device, vk_state->samplers->nearestClampEdge, vk_state->vkAllocator);
		vkDestroySampler(vk_state->device, vk_state->samplers->nearestRepeat, vk_state->vkAllocator);
		vkDestroySampler(vk_state->device, vk_state->samplers->linearClampEdge, vk_state->vkAllocator);
		vkDestroySampler(vk_state->device, vk_state->samplers->linearRepeat, vk_state->vkAllocator);
		vkDestroySampler(vk_state->device, vk_state->samplers->shadow, vk_state->vkAllocator);

		Free(vk_state->rendererAllocator, vk_state->samplers);
	}

	// ============================================================================================================================================================
	// ====================== Destroying descriptor pool if it was created ==============================================================================================
	// ============================================================================================================================================================
	if (vk_state->descriptorPool)
		vkDestroyDescriptorPool(vk_state->device, vk_state->descriptorPool, vk_state->vkAllocator);

	// ============================================================================================================================================================
	// ====================== Destroying swapchain if it was created ==============================================================================================
	// ============================================================================================================================================================
	DestroySwapchain(vk_state);

	// ============================================================================================================================================================
	// ======================== Shutdown vulkan transfer system ============================================================================================================
	// ============================================================================================================================================================
	VulkanTransferShutdown();

	// ============================================================================================================================================================
	// ================================ Shutdown vulkan memory system =================================================================================
	// ============================================================================================================================================================
	VulkanMemoryShutdown();

	// ============================================================================================================================================================
	// ================================ Destroy sync objects if they were created =================================================================================
	// ============================================================================================================================================================
	for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (vk_state->imageAvailableSemaphores[i])
			vkDestroySemaphore(vk_state->device, vk_state->imageAvailableSemaphores[i], vk_state->vkAllocator);
		if (vk_state->prePresentCompleteSemaphores[i])
			vkDestroySemaphore(vk_state->device, vk_state->prePresentCompleteSemaphores[i], vk_state->vkAllocator);
	}

	if (vk_state->frameSemaphore.handle)
		vkDestroySemaphore(vk_state->device, vk_state->frameSemaphore.handle, vk_state->vkAllocator);
	if (vk_state->duplicatePrePresentCompleteSemaphore.handle)
		vkDestroySemaphore(vk_state->device, vk_state->duplicatePrePresentCompleteSemaphore.handle, vk_state->vkAllocator);

	// ============================================================================================================================================================
	// ===================== destroys command pools ====================================================================================================
	// ============================================================================================================================================================
	if (vk_state->graphicsQueue.commandPool)
	{
		vkDestroyCommandPool(vk_state->device, vk_state->graphicsQueue.commandPool, vk_state->vkAllocator);
	}

	if (vk_state->transferQueue.commandPool)
	{
		vkDestroyCommandPool(vk_state->device, vk_state->transferQueue.commandPool, vk_state->vkAllocator);
	}

	if (vk_state->presentQueue.commandPool)
	{
		vkDestroyCommandPool(vk_state->device, vk_state->presentQueue.commandPool, vk_state->vkAllocator);
	}

	ShutdownDeferredResourceDestructionState(&vk_state->deferredResourceDestruction);

	// ============================================================================================================================================================
	// ===================== Destroying logical device if it was created ==========================================================================================
	// ============================================================================================================================================================
	if (vk_state->device)
		vkDestroyDevice(vk_state->device, vk_state->vkAllocator);
	if (vk_state->swapchainSupport.formats)
		Free(vk_state->rendererAllocator, vk_state->swapchainSupport.formats);
	if (vk_state->swapchainSupport.presentModes)
		Free(vk_state->rendererAllocator, vk_state->swapchainSupport.presentModes);

	// ============================================================================================================================================================
	// ======================= Destroying the surface if it was created ===========================================================================================
	// ============================================================================================================================================================
	if (vk_state->surface)
		vkDestroySurfaceKHR(vk_state->instance, vk_state->surface, vk_state->vkAllocator);

	// ============================================================================================================================================================
	// ===================== Destroying debug messenger if it was created =========================================================================================
	// ============================================================================================================================================================
	DestroyDebugMessenger();

	// ============================================================================================================================================================
	// ======================= Destroying instance if it was created ==============================================================================================
	// ============================================================================================================================================================
	if (vk_state->instance)
		vkDestroyInstance(vk_state->instance, vk_state->vkAllocator);

	DarrayDestroy(vk_state->mipGenerationQueue);
	DestroyFreelistAllocator(vk_state->rendererAllocator);
	Free(GetGlobalAllocator(), vk_state);
	vk_state = nullptr;
}

void RecreateSwapchain()
{
	VK_CHECK(vkDeviceWaitIdle(vk_state->device));

	DestroySwapchain(vk_state);

	CreateSwapchain(vk_state->requestedPresentMode);

	vk_state->shouldRecreateSwapchain = false;
	_INFO("Vulkan Swapchain resized");

	EventData eventData = {};
	InvokeEvent(EVCODE_SWAPCHAIN_RESIZED, eventData);
}

bool BeginRendering()
{
	// Destroy temporary resources that the GPU has finished with (e.g. staging buffers, etc.)
	TryDestroyResourcesPendingDestruction();

	// Recreating the swapchain if the window has been resized
	if (vk_state->shouldRecreateSwapchain)
		RecreateSwapchain();

	// TODO: temporary fix for synch issues
	//vkDeviceWaitIdle(vk_state->device);

	// ================================= Waiting for rendering resources to become available ==============================================================
	// The GPU can work on multiple frames simultaneously (i.e. multiple frames can be "in flight"), but each frame has it's own resources
	// that the GPU needs while it's rendering a frame. So we need to wait for one of those sets of resources to become available again (command buffers and binary semaphores).
#define CPU_SIDE_WAIT_SEMAPHORE_COUNT 2
	VkSemaphore waitSemaphores[CPU_SIDE_WAIT_SEMAPHORE_COUNT] = { vk_state->frameSemaphore.handle, vk_state->duplicatePrePresentCompleteSemaphore.handle };
	u64 waitValues[CPU_SIDE_WAIT_SEMAPHORE_COUNT] = { vk_state->frameSemaphore.submitValue - (MAX_FRAMES_IN_FLIGHT - 1), vk_state->duplicatePrePresentCompleteSemaphore.submitValue - (MAX_FRAMES_IN_FLIGHT - 1) };

	VkSemaphoreWaitInfo semaphoreWaitInfo = {};
	semaphoreWaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	semaphoreWaitInfo.pNext = nullptr;
	semaphoreWaitInfo.flags = 0;
	semaphoreWaitInfo.semaphoreCount = CPU_SIDE_WAIT_SEMAPHORE_COUNT;
	semaphoreWaitInfo.pSemaphores = waitSemaphores;
	semaphoreWaitInfo.pValues = waitValues;

	VK_CHECK(vkWaitSemaphores(vk_state->device, &semaphoreWaitInfo, UINT64_MAX));

	// Transferring resources to the GPU
	VulkanCommitTransfers();

	// Getting the next image from the swapchain (doesn't block the CPU and only blocks the GPU if there's no image available (which only happens in certain present modes with certain buffer counts))
	VkResult result = vkAcquireNextImageKHR(vk_state->device, vk_state->swapchain, UINT64_MAX, vk_state->imageAvailableSemaphores[vk_state->currentInFlightFrameIndex], VK_NULL_HANDLE, &vk_state->currentSwapchainImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		vk_state->shouldRecreateSwapchain = true;
		return false;
	}
	else if (result == VK_SUBOPTIMAL_KHR)
	{
		// Sets recreate swapchain to true BUT DOES NOT RETURN because the image has been acquired so we can continue rendering for this frame
		vk_state->shouldRecreateSwapchain = true;
	}
	else if (result != VK_SUCCESS)
	{
		_WARN("Failed to acquire next swapchain image");
		return false;
	}

	// ===================================== Begin command buffer recording =========================================
	ResetAndBeginCommandBuffer(vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex]);
	VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

	// =============================== acquire ownership of all uploaded resources =======================================
	vkCmdPipelineBarrier2(currentCommandBuffer, vk_state->transferState.uploadAcquireDependencyInfo);
	vk_state->transferState.uploadAcquireDependencyInfo = nullptr;
	INSERT_DEBUG_MEMORY_BARRIER(currentCommandBuffer);

	// Generating mips for newly uploaded images that need them
	if (vk_state->mipGenerationQueue->size > 0)
		GenerateMips();

	// Binding global ubo
	VulkanShader* defaultShader = SimpleMapLookup(vk_state->shaderMap, DEFAULT_SHADER_NAME);
	vkCmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, defaultShader->pipelineLayout, 0, 1, &vk_state->globalDescriptorSetArray[vk_state->currentInFlightFrameIndex], 0, nullptr);

	return true;
}

void EndRendering()
{
	VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

	// ====================================== Transition swapchain image to transfer dst ======================================================
	{
		VkImageMemoryBarrier2 rendertargetTransitionImageBarrierInfo = {};
		rendertargetTransitionImageBarrierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		rendertargetTransitionImageBarrierInfo.pNext = nullptr;
		rendertargetTransitionImageBarrierInfo.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		rendertargetTransitionImageBarrierInfo.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
		rendertargetTransitionImageBarrierInfo.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
		rendertargetTransitionImageBarrierInfo.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		rendertargetTransitionImageBarrierInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		rendertargetTransitionImageBarrierInfo.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		rendertargetTransitionImageBarrierInfo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		rendertargetTransitionImageBarrierInfo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		rendertargetTransitionImageBarrierInfo.image = vk_state->swapchainImages[vk_state->currentSwapchainImageIndex];
		rendertargetTransitionImageBarrierInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		rendertargetTransitionImageBarrierInfo.subresourceRange.baseMipLevel = 0;
		rendertargetTransitionImageBarrierInfo.subresourceRange.levelCount = 1;
		rendertargetTransitionImageBarrierInfo.subresourceRange.baseArrayLayer = 0;
		rendertargetTransitionImageBarrierInfo.subresourceRange.layerCount = 1;

		VkDependencyInfo rendertargetTransitionDependencyInfo = {};
		rendertargetTransitionDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		rendertargetTransitionDependencyInfo.pNext = nullptr;
		rendertargetTransitionDependencyInfo.dependencyFlags = 0;
		rendertargetTransitionDependencyInfo.memoryBarrierCount = 0;
		rendertargetTransitionDependencyInfo.bufferMemoryBarrierCount = 0;
		rendertargetTransitionDependencyInfo.imageMemoryBarrierCount = 1;
		rendertargetTransitionDependencyInfo.pImageMemoryBarriers = &rendertargetTransitionImageBarrierInfo;

		vkCmdPipelineBarrier2(currentCommandBuffer, &rendertargetTransitionDependencyInfo);
	}

	VulkanRenderTarget* mainRenderTarget = vk_state->mainRenderTarget.internalState;

	VkImageBlit2 blitRegion = {};
	blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blitRegion.pNext = nullptr;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.mipLevel = 0;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;

	blitRegion.srcOffsets[1].x = mainRenderTarget->extent.width;
	blitRegion.srcOffsets[1].y = mainRenderTarget->extent.height;
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.mipLevel = 0;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;

	blitRegion.dstOffsets[1].x = vk_state->swapchainExtent.width;
	blitRegion.dstOffsets[1].y = vk_state->swapchainExtent.height;
	blitRegion.dstOffsets[1].z = 1;

	VkBlitImageInfo2 blitInfo = {};
	blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blitInfo.pNext = nullptr;
	blitInfo.srcImage = mainRenderTarget->colorImage.handle;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.dstImage = vk_state->swapchainImages[vk_state->currentSwapchainImageIndex];
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;
	blitInfo.filter = VK_FILTER_LINEAR;

	vkCmdBlitImage2(currentCommandBuffer, &blitInfo);

	// ====================================== Transition swapchain image to present ready and releasing from graphics queue ======================================================
	{
		VkImageMemoryBarrier2 swapchainImageTransitionReleaseImageBarrierInfo = {};
		swapchainImageTransitionReleaseImageBarrierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		swapchainImageTransitionReleaseImageBarrierInfo.pNext = nullptr;
		swapchainImageTransitionReleaseImageBarrierInfo.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
		swapchainImageTransitionReleaseImageBarrierInfo.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		swapchainImageTransitionReleaseImageBarrierInfo.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		swapchainImageTransitionReleaseImageBarrierInfo.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		swapchainImageTransitionReleaseImageBarrierInfo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		swapchainImageTransitionReleaseImageBarrierInfo.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		swapchainImageTransitionReleaseImageBarrierInfo.srcQueueFamilyIndex = vk_state->graphicsQueue.index;
		swapchainImageTransitionReleaseImageBarrierInfo.dstQueueFamilyIndex = vk_state->presentQueue.index;
		swapchainImageTransitionReleaseImageBarrierInfo.image = vk_state->swapchainImages[vk_state->currentSwapchainImageIndex];
		swapchainImageTransitionReleaseImageBarrierInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		swapchainImageTransitionReleaseImageBarrierInfo.subresourceRange.baseMipLevel = 0;
		swapchainImageTransitionReleaseImageBarrierInfo.subresourceRange.levelCount = 1;
		swapchainImageTransitionReleaseImageBarrierInfo.subresourceRange.baseArrayLayer = 0;
		swapchainImageTransitionReleaseImageBarrierInfo.subresourceRange.layerCount = 1;

		VkDependencyInfo swapchainImageTransitionDependencyInfo = {};
		swapchainImageTransitionDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		swapchainImageTransitionDependencyInfo.pNext = nullptr;
		swapchainImageTransitionDependencyInfo.dependencyFlags = 0;
		swapchainImageTransitionDependencyInfo.memoryBarrierCount = 0;
		swapchainImageTransitionDependencyInfo.bufferMemoryBarrierCount = 0;
		swapchainImageTransitionDependencyInfo.imageMemoryBarrierCount = 1;
		swapchainImageTransitionDependencyInfo.pImageMemoryBarriers = &swapchainImageTransitionReleaseImageBarrierInfo;

		vkCmdPipelineBarrier2(currentCommandBuffer, &swapchainImageTransitionDependencyInfo);
	}

	// ================================= End graphics command buffer recording ==================================================
	EndCommandBuffer(vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex]);

	// =================================== Submitting graphics command buffer ==============================================
	{
		// With all the synchronization that that entails...
		const u32 waitSemaphoreCount = 2; // 1 swapchain image acquisition, 1 resourse upload waits
		VkSemaphoreSubmitInfo waitSemaphores[waitSemaphoreCount] = {};

		// Swapchain image acquisition semaphore
		waitSemaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitSemaphores[0].pNext = nullptr;
		waitSemaphores[0].semaphore = vk_state->imageAvailableSemaphores[vk_state->currentInFlightFrameIndex];
		waitSemaphores[0].value = 0;
		waitSemaphores[0].stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
		waitSemaphores[0].deviceIndex = 0;

		// Resource upload semaphores
		waitSemaphores[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitSemaphores[1].pNext = nullptr;
		waitSemaphores[1].semaphore = vk_state->transferState.uploadSemaphore.handle;
		waitSemaphores[1].value = vk_state->transferState.uploadSemaphore.submitValue;
		waitSemaphores[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		waitSemaphores[1].deviceIndex = 0;

		const u32 signalSemaphoreCount = 1;
		VkSemaphoreSubmitInfo signalSemaphores[signalSemaphoreCount] = {};

		vk_state->frameSemaphore.submitValue++;
		signalSemaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalSemaphores[0].pNext = nullptr;
		signalSemaphores[0].semaphore = vk_state->frameSemaphore.handle;
		signalSemaphores[0].value = vk_state->frameSemaphore.submitValue;
		signalSemaphores[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		signalSemaphores[0].deviceIndex = 0;

		// Submitting the command buffer which allows the GPU to actually start working on this frame
		SubmitCommandBuffers(waitSemaphoreCount, waitSemaphores, signalSemaphoreCount, signalSemaphores, 1, &vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex], nullptr);
	}

	// ============================== Telling the GPU to present this frame (after it's rendered of course, synced with a binary semaphore) =================================
	// First acquiring ownership (present queue) of the swapchain image that is to be presented.
	{
		ResetAndBeginCommandBuffer(vk_state->presentCommandBuffers[vk_state->currentInFlightFrameIndex]);
		VkCommandBuffer presentCommandBuffer = vk_state->presentCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

		// Image memory barrier for transitioning to present and acquiring on present queue
		{
			VkImageMemoryBarrier2 swapchainImageTransitionImageBarrierInfo = {};
			swapchainImageTransitionImageBarrierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			swapchainImageTransitionImageBarrierInfo.pNext = nullptr;
			swapchainImageTransitionImageBarrierInfo.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
			swapchainImageTransitionImageBarrierInfo.srcAccessMask = VK_ACCESS_2_NONE;
			swapchainImageTransitionImageBarrierInfo.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			swapchainImageTransitionImageBarrierInfo.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
			swapchainImageTransitionImageBarrierInfo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			swapchainImageTransitionImageBarrierInfo.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			swapchainImageTransitionImageBarrierInfo.srcQueueFamilyIndex = vk_state->graphicsQueue.index;
			swapchainImageTransitionImageBarrierInfo.dstQueueFamilyIndex = vk_state->presentQueue.index;
			swapchainImageTransitionImageBarrierInfo.image = vk_state->swapchainImages[vk_state->currentSwapchainImageIndex];
			swapchainImageTransitionImageBarrierInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			swapchainImageTransitionImageBarrierInfo.subresourceRange.baseMipLevel = 0;
			swapchainImageTransitionImageBarrierInfo.subresourceRange.levelCount = 1;
			swapchainImageTransitionImageBarrierInfo.subresourceRange.baseArrayLayer = 0;
			swapchainImageTransitionImageBarrierInfo.subresourceRange.layerCount = 1;

			VkDependencyInfo swapchainImageTransitionDependencyInfo = {};
			swapchainImageTransitionDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			swapchainImageTransitionDependencyInfo.pNext = nullptr;
			swapchainImageTransitionDependencyInfo.dependencyFlags = 0;
			swapchainImageTransitionDependencyInfo.memoryBarrierCount = 0;
			swapchainImageTransitionDependencyInfo.bufferMemoryBarrierCount = 0;
			swapchainImageTransitionDependencyInfo.imageMemoryBarrierCount = 1;
			swapchainImageTransitionDependencyInfo.pImageMemoryBarriers = &swapchainImageTransitionImageBarrierInfo;

			vkCmdPipelineBarrier2(presentCommandBuffer, &swapchainImageTransitionDependencyInfo);
		}

		const u32 waitSemaphoreCount = 1; // 1 swapchain image queue acquisition
		VkSemaphoreSubmitInfo waitSemaphores[waitSemaphoreCount] = {};

		// Swapchain image acquisition semaphore
		waitSemaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitSemaphores[0].pNext = nullptr;
		waitSemaphores[0].semaphore = vk_state->frameSemaphore.handle;
		waitSemaphores[0].value = vk_state->frameSemaphore.submitValue;
		waitSemaphores[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		waitSemaphores[0].deviceIndex = 0;

		const u32 signalSemaphoreCount = 2;
		VkSemaphoreSubmitInfo signalSemaphores[signalSemaphoreCount] = {};
		signalSemaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalSemaphores[0].pNext = nullptr;
		signalSemaphores[0].semaphore = vk_state->prePresentCompleteSemaphores[vk_state->currentInFlightFrameIndex];
		signalSemaphores[0].value = 0;
		signalSemaphores[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		signalSemaphores[0].deviceIndex = 0;

		vk_state->duplicatePrePresentCompleteSemaphore.submitValue++;
		signalSemaphores[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalSemaphores[1].pNext = nullptr;
		signalSemaphores[1].semaphore = vk_state->duplicatePrePresentCompleteSemaphore.handle;
		signalSemaphores[1].value = vk_state->duplicatePrePresentCompleteSemaphore.submitValue;
		signalSemaphores[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		signalSemaphores[1].deviceIndex = 0;

		EndCommandBuffer(vk_state->presentCommandBuffers[vk_state->currentInFlightFrameIndex]);

		SubmitCommandBuffers(waitSemaphoreCount, waitSemaphores, signalSemaphoreCount, signalSemaphores, 1, &vk_state->presentCommandBuffers[vk_state->currentInFlightFrameIndex], nullptr);
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &vk_state->prePresentCompleteSemaphores[vk_state->currentInFlightFrameIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vk_state->swapchain;
	presentInfo.pImageIndices = &vk_state->currentSwapchainImageIndex;
	presentInfo.pResults = nullptr;

	// When using mailbox present mode, vulkan will take care of skipping the presentation of this frame if another one is already finished
	VK_CHECK(vkQueuePresentKHR(vk_state->presentQueue.handle, &presentInfo));

	vk_state->currentFrameIndex += 1;
	vk_state->currentInFlightFrameIndex = (vk_state->currentInFlightFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void UpdateGlobalUniform(GlobalUniformObject* properties)
{
	CopyDataToAllocation(&vk_state->globalUniformAllocationArray[vk_state->currentInFlightFrameIndex], properties, 0, sizeof(*properties));
}

void Draw(u32 vertexBufferCount, VertexBuffer* clientVertexBuffers, IndexBuffer clientIndexBuffer, mat4* pushConstantValues, u32 instanceCount)
{
	VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;
	VulkanIndexBuffer* indexBuffer = clientIndexBuffer.internalState;

	GRASSERT_DEBUG(vertexBufferCount <= MAX_VERTEX_BUFFERS_PER_DRAW_CALL);

	// Getting vertex buffer vulkan handles for binding
	VkBuffer vertexBuffers[MAX_VERTEX_BUFFERS_PER_DRAW_CALL] = {};
	for (int i = 0; i < vertexBufferCount; i++)
	{
		VulkanVertexBuffer* vb = clientVertexBuffers[i].internalState;
		vertexBuffers[i] = vb->handle;
	}

	// binding index and vertex buffers
	VkDeviceSize offsets[2] = { 0, 0 };
	vkCmdBindVertexBuffers(currentCommandBuffer, 0, vertexBufferCount, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(currentCommandBuffer, indexBuffer->handle, offsets[0], VK_INDEX_TYPE_UINT32);

	if (pushConstantValues)
		vkCmdPushConstants(currentCommandBuffer, vk_state->boundShader->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(*pushConstantValues), pushConstantValues);
	vkCmdDrawIndexed(currentCommandBuffer, indexBuffer->indexCount, instanceCount, 0, 0, 0);
}

void DrawBufferRange(u32 vertexBufferCount, VertexBuffer* clientVertexBuffers, u64* vbOffsets, IndexBuffer clientIndexBuffer, mat4* pushConstantValues, u32 instanceCount)
{
	VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;
	VulkanIndexBuffer* indexBuffer = clientIndexBuffer.internalState;

	GRASSERT_DEBUG(vertexBufferCount <= MAX_VERTEX_BUFFERS_PER_DRAW_CALL);

	// Getting vertex buffer vulkan handles for binding
	VkBuffer vertexBuffers[MAX_VERTEX_BUFFERS_PER_DRAW_CALL] = {};
	VkDeviceSize offsets[2] = { 0, 0 };
	for (int i = 0; i < vertexBufferCount; i++)
	{
		VulkanVertexBuffer* vb = clientVertexBuffers[i].internalState;
		vertexBuffers[i] = vb->handle;
		offsets[i] = vbOffsets[i];
	}

	// binding index and vertex buffers
	vkCmdBindVertexBuffers(currentCommandBuffer, 0, vertexBufferCount, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(currentCommandBuffer, indexBuffer->handle, 0, VK_INDEX_TYPE_UINT32);

	if (pushConstantValues)
		vkCmdPushConstants(currentCommandBuffer, vk_state->boundShader->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(*pushConstantValues), pushConstantValues);
	vkCmdDrawIndexed(currentCommandBuffer, indexBuffer->indexCount, instanceCount, 0, 0, 0);
}

RenderTarget GetMainRenderTarget()
{
	return vk_state->mainRenderTarget;
}

GPUMesh* GetBasicMesh(const char* meshName)
{
	return SimpleMapLookup(vk_state->basicMeshMap, meshName);
}

vec4 ScreenToClipSpace(vec4 coordinates)
{
	vec2i windowSize = GetPlatformWindowSize();

	coordinates.x = coordinates.x / (f32)windowSize.x;
	coordinates.y = coordinates.y / (f32)windowSize.y;
	coordinates.x = coordinates.x * 2;
	coordinates.y = coordinates.y * 2;
	coordinates.x -= 1;
	coordinates.y -= 1;

	return coordinates;
}

static bool OnWindowResize(EventCode type, EventData data)
{
	vk_state->shouldRecreateSwapchain = true;
	return false;
}
