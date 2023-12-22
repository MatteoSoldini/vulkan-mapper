#pragma once
#include <string>
#include <vector>
#include "vk_state.h"
#include "video.h"
#include "vm_types.h"
#include "media.h"

class Video;

class VulkanState;

class MediaManager {
private:
	VulkanState* pEngine;
	std::vector<Media*> medias;
	MediaId_t newId();

public:
	MediaManager(VulkanState* pEngine);

	void loadFile(std::string filePath);
	
	// video decode loop
	void decodeLoop();

	std::vector<MediaId_t> getMediasIds();
	Media* getMediaById(MediaId_t mediaId);
};