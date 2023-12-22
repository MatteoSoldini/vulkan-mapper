#include "../include/media_manager.h"
//#include "../include/vk_video.h"
#include "../include/read_file.h"

#include <stdexcept>
#include <minimp4.h>
#include <iostream>
#include <algorithm>
#include "../include/image.h"


MediaManager::MediaManager(VulkanState* pEngine) {
    MediaManager::pEngine = pEngine;
}

void MediaManager::loadFile(std::string filePath) {
    std::string fileExtension = filePath.substr(filePath.find_last_of(".") + 1);

    if (fileExtension == "mp4") {
        medias.push_back(new Video(newId(), pEngine, filePath));
    }
    else if (fileExtension == "jpg" || fileExtension == "png") {
        medias.push_back(new Image(newId(), pEngine, filePath));
    }
}

MediaId_t MediaManager::newId() {
    MediaId_t newId = 0;

    // find new id
    for (auto media : medias) {
        if (media->getId() >= newId) {
            newId = media->getId() + 1;
        }
    }

    return newId;
}

void MediaManager::decodeLoop() {
    // check for any video that need decoding
    for (auto media : medias) {
        if (auto pVideo = dynamic_cast<Video*>(media)) {   // VIDEO
            pVideo->decodeFrame();
        }
    }
}

std::vector<MediaId_t> MediaManager::getMediasIds() {
    std::vector<MediaId_t> mediasIds;

    for (auto media : medias) {
        mediasIds.push_back(media->getId());
    }

    return mediasIds;
}

Media* MediaManager::getMediaById(MediaId_t mediaId) {
    for (int i = 0; i < medias.size(); i++) {
        if (medias[i]->getId() == mediaId) {
            return medias[i];
        }
    }

    return nullptr;
}