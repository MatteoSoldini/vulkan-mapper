#pragma once
#include <string>
#include <vector>
#include "vk_engine.h"
#include <chrono>
#include "video.h"

class Video;

typedef uint8_t m_id;

enum MediaType {
	IMAGE,
	VIDEO,
};

struct Media {
	m_id id;
	MediaType type;
	std::string filePath;
	Video* pState = nullptr;
};

class MediaManager {
private:
	VulkanEngine* pEngine;
	std::vector<Media> medias;
	m_id newId();

	// image
	void loadImage(std::string filePath);

	// video
	void loadVideo(std::string filePath);

public:
	MediaManager(VulkanEngine* pEngine);

	void loadFile(std::string filePath);
	
	// video decode loop
	void decodeFrames();

	std::vector<Media> getMedias();
	Media* getMediaById(m_id mediaId);

};