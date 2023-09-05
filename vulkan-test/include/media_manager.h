#pragma once
#include <string>
#include <vector>
#include "vk_engine.h"

class VulkanEngine;

struct MediaImage {
	uint8_t id;
	std::string filePath;
};

class MediaManager {
private:
	VulkanEngine* pEngine;
	std::vector<MediaImage> images;

public:
	MediaManager(VulkanEngine* pEngine) {
		MediaManager::pEngine = pEngine;
	}

	void loadImage(std::string filePath);

	std::vector<MediaImage> getMedias();
};