#pragma once
#include <vulkan/vulkan.h>
#include "defines.h"
#include "containers/darray.h"
#include "containers/circular_queue.h"
#include "../buffer.h"
#include "../render_target.h"
#include "containers/simplemap.h"
#include "../renderer.h"
#include "core/asserts.h"

typedef struct RendererState RendererState;
extern RendererState* vk_state;

#define MAX_SHADERS 256
#define BASIC_MESH_COUNT 4
#define MAX_FRAMES_IN_FLIGHT 2
#define RENDER_POOL_BLOCK_SIZE_32 32
#define QUEUE_ACQUISITION_POOL_BLOCK_SIZE 160 // 160 bytes (2.5 cache lines) 32 byte aligned, enough to store VkDependencyInfo + (VkImageMemoryBarrier2 or VkBufferMemoryBarrier2)
#define VULKAN_FRAME_ARENA_BYTES (MiB * 10)

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			_ERROR("Detected vulkan error: %s", err);				\
			debugBreak();                                           \
		}                                                           \
	} while (0)


typedef enum VulkanMemoryType
{
	MEMORY_TYPE_STATIC = 0,
	MEMORY_TYPE_UPLOAD = 1,
	MEMORY_TYPE_DYNAMIC = 2,
} VulkanMemoryType;

typedef struct VkMemoryTypeHolder
{
	VulkanMemoryType memoryType;
} VkMemoryTypeHolder;

static inline VkMemoryTypeHolder MemType(VulkanMemoryType type) { return (VkMemoryTypeHolder){type}; }

typedef struct VulkanAllocation
{
	VkDeviceMemory deviceMemory;			// The device memory this allocation is suballocated in
	VkDeviceSize userAllocationSize;		// Size of the allocation from the user perspective
	VkDeviceSize userAllocationOffset;		// Offset from the start of the allocation from the allocator perspective to the start of the allocation from the user perspective (this offset usually exists to have correct alignment)
	VkDeviceSize address;					// Address of the allocation from the user perspective
	void* mappedMemory;						// Mapped memory, this pointer points to the start of the allocation from the user perspective, nullptr if the allocation is not on HOST_VISIBLE memory
	u32 memoryType;
} VulkanAllocation;

typedef struct VulkanVertexBuffer
{
	VkDeviceSize size;
	VkBuffer handle;
	VulkanAllocation memory;
} VulkanVertexBuffer;

typedef struct VulkanIndexBuffer
{
	VkDeviceSize size;
	VkBuffer handle;
	VulkanAllocation memory;
	size_t indexCount;
} VulkanIndexBuffer;

typedef struct VulkanImage
{
	VkImage handle;
	VkImageView view;
	VulkanAllocation memory;
} VulkanImage;

typedef struct VulkanBufferCopyData
{
	VkBuffer dstBuffer;
	VkBuffer srcBuffer;
    VkBufferCopy copyRegion;
} VulkanBufferCopyData;

typedef struct VulkanBufferToImageUploadData
{
	VkBuffer srcBuffer;
	VkImage dstImage;
	u32 imageWidth;
	u32 imageHeight;
} VulkanBufferToImageUploadData;

typedef struct VulkanRenderTarget
{
	VkExtent2D extent;
	RenderTargetUsage colorBufferUsage;
	RenderTargetUsage depthBufferUsage;
	VulkanImage colorImage;
	VulkanImage depthImage;
} VulkanRenderTarget;

#define PROPERTY_MAX_NAME_LENGTH 20

typedef struct UniformPropertiesData
{
	u32 propertyCount;									// Amount of properties
	u32 uniformBufferSize;								// Amount of bytes the uniforms take up
	char* propertyStringsMemory;						// Backing memory for the propertyNameArray, PROPERTY_MAX_NAME_LENGTH chars per property
	char** propertyNameArray;							// Array of property names
	u32* propertyOffsets;								// Array of property memory offsets
	u32* propertySizes;									// Array of property sizes
	u32 bindingIndex;									// Binding of this uniform buffer
} UniformPropertiesData;

typedef struct UniformTexturesData
{
	u32 textureCount;									// Amount of textures
	char* textureStringsMemory;							// Backing memory for the propertyNameArray, PROPERTY_MAX_NAME_LENGTH chars per property
	char** textureNameArray;							// Array of property names
	u32* bindingIndices;								// Binding indices of the textures
} UniformTexturesData;

typedef struct VulkanShader
{
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipelineObject;
	UniformPropertiesData vertUniformPropertiesData;
	UniformPropertiesData fragUniformPropertiesData;
	UniformTexturesData vertUniformTexturesData;
	UniformTexturesData fragUniformTexturesData;
	u32 totalUniformDataSize;
	u32 fragmentUniformBufferOffset;
} VulkanShader;

typedef struct VulkanMaterial
{
	VulkanShader* shader;								// Handle to the shader this material is an instance of
	VkBuffer uniformBuffer;								// VkBuffer that backs this materials uniforms
	VulkanAllocation uniformBufferAllocation;			// Allocation that backs this materials uniforms
	VkDescriptorSet* descriptorSetArray;				// Descriptor sets
} VulkanMaterial;

typedef struct VulkanSemaphore
{
	VkSemaphore handle;
	u64 submitValue;
} VulkanSemaphore;

typedef enum DestructionObjectType
{
	DESTRUCTION_OBJECT_TYPE_IMAGE,
	DESTRUCTION_OBJECT_TYPE_BUFFER,
} DestructionObjectType;

typedef struct ResourceDestructionInfo
{
	void* resource0;
	void* resource1;
	u64 signalValue;
	VulkanAllocation allocation;
	DestructionObjectType destructionObjectType;
} ResourceDestructionInfo;

DEFINE_DARRAY_TYPE(ResourceDestructionInfo);
DEFINE_CIRCULARQUEUE_TYPE(ResourceDestructionInfo);

typedef struct DeferResourceDestructionState
{
	ResourceDestructionInfoCircularQueue destructionQueue;
	ResourceDestructionInfoDarray* destructionOverflowDarray;
} DeferResourceDestructionState;

typedef struct QueueFamily
{
	VkQueue handle;
	VkCommandPool commandPool;
	u32 index;
} QueueFamily;

typedef struct CommandBuffer
{
	VkCommandBuffer handle;
	QueueFamily* queueFamily;
} CommandBuffer;

typedef struct SwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	u32 formatCount;
	VkSurfaceFormatKHR* formats;
	u32 presentModeCount;
	VkPresentModeKHR* presentModes;
} SwapchainSupportDetails;

typedef struct VulkanSamplers
{
	VkSampler nearestClampEdge;
	VkSampler nearestRepeat;
	VkSampler linearClampEdge;
	VkSampler linearRepeat;
	VkSampler shadow;				// Sampler with comparisson state enabled for percentage closer filtering
} VulkanSamplers;

typedef struct HeapInfo
{
	VkDeviceSize heapCapacity;		// Not the actual heap size, but the heap size times a factor less than one, depending on the type of heap. (to account for the fact that others might be using the gpu)
	VkDeviceSize heapUsage;			// How much memory the application has allocated from this heap
} HeapInfo;

typedef struct VulkanFreelistNode
{
    VkDeviceSize address;              // Address of this free block
    VkDeviceSize size;                // Size of this free block
    struct VulkanFreelistNode* next;  // Pointer to the freelist node after this 
} VulkanFreelistNode;

typedef struct VulkanAllocatorMemoryBlock
{
	VkDeviceMemory deviceMemory;
	void* mappedMemory;				// Is nullptr if this block is not host visible
	VkDeviceSize size;
	VulkanFreelistNode* head;         // The first free node in the arena
    VulkanFreelistNode* nodePool;     // Address of the array that is used for the freelist node pool
    u32 nodeCount;              // Amount of free nodes the allocator can have
} VulkanAllocatorMemoryBlock;

typedef struct VulkanFreelistAllocator
{
	VulkanAllocatorMemoryBlock* memoryBlocks;
	u32 heapIndex;
	u32 memoryTypeIndex;
	u32 memoryBlockCount;
} VulkanFreelistAllocator;

typedef struct VulkanMemoryState
{
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	Allocator* vulkanAllocatorStateAllocator;			// Allocator that is used for allocating the state of VulkanFreelistAllocators and VulkanAllocatorMemoryBlocks
	VulkanFreelistAllocator* smallBufferAllocators;
	VulkanFreelistAllocator* largeBufferAllocators;
	VulkanFreelistAllocator* imageAllocators;
	HeapInfo* heapInfos;
	u32 heapCount;
	u32 memoryTypeCount;								// Also represents the amount of freelist allocators in the VulkanFreelistAllocator arrays
} VulkanMemoryState;

typedef enum TransferMethod
{
	TRANSFER_METHOD_UNSYNCHRONIZED,
	TRANSFER_METHOD_SYNCHRONIZED_DOUBLE_BUFFERED,
	TRANSFER_METHOD_SYNCHRONIZED_SINGLE_BUFFERED,
} TransferMethod;

DEFINE_DARRAY_TYPE(VulkanBufferCopyData);
DEFINE_DARRAY_TYPE(VulkanBufferToImageUploadData);

typedef struct TransferState
{
	VulkanBufferCopyDataDarray* bufferCopyOperations;
	VulkanBufferToImageUploadDataDarray* bufferToImageCopyOperations;
	CommandBuffer transferCommandBuffers[MAX_FRAMES_IN_FLIGHT];
	VulkanSemaphore uploadSemaphore;
	VkDependencyInfo* uploadAcquireDependencyInfo;
	TransferMethod slowestTransferMethod;
} TransferState;

typedef struct RendererState
{
	// Frequently used data (every frame)
	VkDevice device;												// Logical device
	VkSwapchainKHR swapchain;										// Swapchain handle
	CommandBuffer graphicsCommandBuffers[MAX_FRAMES_IN_FLIGHT]; 	// Command buffers for recording entire frames to the graphics queue
	CommandBuffer presentCommandBuffers[MAX_FRAMES_IN_FLIGHT]; 		// Command buffers for transferring ownership of present images to the present queue
	u64 currentFrameIndex;											// Current frame
	u32 currentInFlightFrameIndex;									// Current frame % MAX_FRAMES_IN_FLIGHT
	u32 currentSwapchainImageIndex;									// Current swapchain image index (current frame % swapchain image count)
	VkImage* swapchainImages;										// Images that make up the swapchain (retrieved from the swapchain)
	VkImageView* swapchainImageViews;								// Image views that make up the swapchain (created from swapchain images)
	bool shouldRecreateSwapchain;									// Checked at the start of each renderloop, is set to true upon window resize
	VkExtent2D swapchainExtent;										// Extent of the swapchain, used for beginning renderpass
	VulkanShader* boundShader;										// Currently bound shader (pipeline object)
	VkDescriptorSet* globalDescriptorSetArray;						// Global descriptor set array, one per possible in flight frame
	RenderTarget mainRenderTarget;									// Render target used for rendering the main scene
	TransferState transferState;
	DeferResourceDestructionState deferredResourceDestruction;
	QueueFamily graphicsQueue;										// Graphics family queue
	QueueFamily transferQueue;										// Transfer family queue
	QueueFamily presentQueue;										// Present family queue

	// Binary semaphores for synchronizing the swapchain with the screen and the GPU
	VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];		// Binary semaphores that synchronize swapchain image acquisition TODO: change to timeline semaphore once vulkan allows it (hopefully 1.5)
	VkSemaphore prePresentCompleteSemaphores[MAX_FRAMES_IN_FLIGHT];	// Binary semaphores that synchronize swapchain image presentation TODO: change to timeline semaphore once vulkan allows it (hopefully 1.5)

	// Timeline semaphores for synchronizing uploads and 
	VulkanSemaphore frameSemaphore;									// Timeline semaphore that synchronizes rendering resources
	VulkanSemaphore duplicatePrePresentCompleteSemaphore;			// Timeline semaphore that could replace prePresentCompleteSemaphores if surfaces allowed it TODO:

	// Allocators
	Allocator* rendererAllocator;									// Global allocator of the renderer subsys

	// Data that is not used every frame or possibly used every frame
	VkAllocationCallbacks* vkAllocator;								// Vulkan API allocator, only for reading vulkan allocations not for taking over allocation from vulkan //TODO: this is currently just nullptr
	VulkanMemoryState* vkMemory;									// State for the system that manages gpu memory
	VkDescriptorPool descriptorPool;								// Pool used to allocate descriptor sets for all materials
	Material defaultMaterial;										// Material based on default shader
	VulkanSamplers* samplers;										// All the different texture samplers
	SimpleMap* shaderMap;											// String hashmap that maps shader names to shader references.
	SimpleMap* basicMeshMap;										// MeshData hashmap that maps basic mesh names to meshes.

	// Data that is only used on startup/shutdown
	VkFormat renderTargetColorFormat;								// Image format used for render target color textures
	VkFormat renderTargetDepthFormat;								// Image format used for render target depth textures
	VkInstance instance;											// Vulkan instance handle
	VkPhysicalDevice physicalDevice;								// Physical device handle
	SwapchainSupportDetails swapchainSupport;						// Data about swapchain capabilities
	VkSurfaceKHR surface;											// Vulkan surface handle
	VkFormat swapchainFormat;										// Format of the swapchain
	u32 swapchainImageCount;										// Amount of images in the swapchain
	Texture defaultTexture;											// Default texture
	VkDescriptorSetLayout globalDescriptorSetLayout;				// Descriptor set layout of the global ubo
	VkBuffer* globalUniformBufferArray;								// Global uniform buffer object
	VulkanAllocation* globalUniformAllocationArray;					// Global uniform allocations
	VkPhysicalDeviceProperties deviceProperties;					// Properties of the physical device
	GrPresentMode requestedPresentMode;
#ifndef GR_DIST
	VkDebugUtilsMessengerEXT debugMessenger;						// Debug messenger, only exists in debug mode
#endif // !GR_DIST
} RendererState;
