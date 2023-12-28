#pragma once

#include "media.h"
#include "vk_state.h"

class Image : public Media {
private:
	VmTextureId_t textureId;
	VulkanState* pDevice;

public:
	Image(MediaId_t id, VulkanState* pDevice, std::string filePath);

	~Image();
};