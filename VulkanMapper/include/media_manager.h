#pragma once
#include <string>
#include <vector>
#include "vk_state.h"
#include "video.h"
#include "vm_types.h"
#include "media.h"
#include "app.h"

class Video;

class VulkanState;

class App;

class MediaManager {
private:
	App* pApp;
	std::vector<Media*> medias;
	MediaId_t newId();
	std::vector<MediaId_t> toRemove;

public:
	MediaManager(App* pApp);

	void loadFile(std::string filePath);
	
	// video decode & remove ops
	// intended to be used inside the main loop before rendering
	void updateMedia();

	std::vector<MediaId_t> getMediasIds();
	Media* getMediaById(MediaId_t mediaId);
	void removeMedia(MediaId_t mediaId);

	void cleanup();
};