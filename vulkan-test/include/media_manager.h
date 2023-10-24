#pragma once
#include <string>
#include <vector>
#include "vk_engine.h"
#include "vk_video.h"

class VulkanVideo;

typedef uint8_t m_id;

enum MediaType {
	Image,
	Video,
};

struct Media {
	m_id id;
	MediaType type;
	std::string filePath;
};

class MediaManager {
private:
	VulkanEngine* pEngine;
	std::vector<Media> medias;
	m_id newId();

	VulkanVideo* video;

public:
	MediaManager(VulkanEngine* pEngine);

	void loadImage(std::string filePath);
	
	// video
	void loadVideo(std::string filePath);
	// TEMP
	void nextFrame();

	void decodeFrames();

	std::vector<Media> getMedias();
	Media* getMediaById(m_id mediaId);

};