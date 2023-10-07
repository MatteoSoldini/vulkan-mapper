#pragma once
#include <string>
#include <vector>
#include "vk_engine.h"

class VulkanEngine;

enum MediaType {
	Image,
	Video,
};

struct Media {
	uint8_t id;
	MediaType type;
	std::string filePath;
};

class MediaManager {
private:
	VulkanEngine* pEngine;
	std::vector<Media> medias;

public:
	MediaManager(VulkanEngine* pEngine);

	void loadImage(std::string filePath);
	void loadVideo(std::string filePath);

	std::vector<Media> getMedias();
};