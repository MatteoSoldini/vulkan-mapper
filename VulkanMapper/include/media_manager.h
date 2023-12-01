#pragma once
#include <string>
#include <vector>
#include "vk_state.h"
#include <chrono>
#include "video.h"
#include "vm_types.h"

class Video;

class VulkanState;

enum MediaType {
	IMAGE,
	VIDEO,
};

struct Media {
	MediaId_t id;
	MediaType type;
	std::string filePath;
	Video* pState = nullptr;
};

class MediaManager {
private:
	VulkanState* pEngine;
	std::vector<Media> medias;
	MediaId_t newId();

	// image
	void loadImage(std::string filePath);

	// video
	void loadVideo(std::string filePath);

public:
	MediaManager(VulkanState* pEngine);

	void loadFile(std::string filePath);
	
	// video decode loop
	void decodeFrames();

	std::vector<Media> getMedias();
	Media* getMediaById(MediaId_t mediaId);

};