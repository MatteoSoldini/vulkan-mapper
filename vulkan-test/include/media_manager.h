#pragma once
#include <string>
#include <vector>
#include "vk_engine.h"
#include "vk_video.h"

class VulkanVideo;

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
	uint8_t newId();

	VulkanVideo* video;

public:
	MediaManager(VulkanEngine* pEngine);

	void loadImage(std::string filePath);
	
	// video
	void loadVideo(std::string filePath);
	// TEMP
	void nextFrame();

	std::vector<Media> getMedias();

};