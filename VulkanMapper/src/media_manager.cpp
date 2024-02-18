#include "../include/media_manager.h"
//#include "../include/vk_video.h"
#include "../include/read_file.h"

#include <stdexcept>
#include <minimp4.h>
#include <iostream>
#include <algorithm>
#include "../include/image.h"


MediaManager::MediaManager(App* pApp) {
    MediaManager::pApp = pApp;
}

void MediaManager::loadFile(std::string filePath) {
    std::string fileExtension = filePath.substr(filePath.find_last_of(".") + 1);

    if (fileExtension == "mp4") {
        medias.push_back(new Video(newId(), pApp->getVulkanState(), filePath));
    }
    else if (fileExtension == "jpg" || fileExtension == "png") {
        medias.push_back(new Image(newId(), pApp->getVulkanState(), filePath));
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

void MediaManager::updateMedia() {
    // video decode
    for (auto media : medias) {
        if (auto pVideo = dynamic_cast<Video*>(media)) {   // VIDEO
            pVideo->decodeFrame();
        }
    }

    // remove media
    for (auto mediaId : toRemove) {
        for (int i = 0; i < medias.size(); i++) {
            if (medias[i]->getId() == mediaId) {
                // clear objects using this media
                std::vector<uint8_t> objectsIds = pApp->getScene()->getIds();
                for (auto objectId : objectsIds) {
                    auto pObject = pApp->getScene()->getObjectPointer(objectId);
                    if (auto pPlane = dynamic_cast<Plane*>(pObject)) {
                        if (pPlane->getMediaId() == mediaId) {
                            pPlane->setMediaId(-1);
                        }
                    }
                }

                delete medias[i];
                medias.erase(medias.begin() + i);
            }
        }
    }
    if (toRemove.size() > 0) toRemove.clear();
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

void MediaManager::removeMedia(MediaId_t mediaId) {
    toRemove.push_back(mediaId);
}

void MediaManager::cleanup() {
    for (auto media : medias) {
        delete media;
    }
}
