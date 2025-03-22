#pragma once
#include "defines.h"
#include "vulkan_types.h"



void VulkanTransferInit();
void VulkanTransferShutdown();

void VulkanCommitTransfers();

void RequestBufferUpload(VulkanBufferCopyData* pCopyRequest, TransferMethod transferMethod);

void RequestImageUpload(VulkanBufferToImageUploadData* pCopyRequest, TransferMethod transferMethod);



