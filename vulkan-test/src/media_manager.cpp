#include "../include/media_manager.h"
//#include "../include/vk_video.h"


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>


MediaManager::MediaManager(VulkanEngine* pEngine) {
    MediaManager::pEngine = pEngine;
}

uint8_t MediaManager::newId() {
    uint8_t newId = 0;

    // find new id
    for (auto media : medias) {
        if (media.id >= newId) {
            newId = media.id + 1;
        }
    }

    return newId;
}


void MediaManager::loadImage(std::string filePath) {
    uint8_t id = newId();
    
    // read pixel data
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    // load to engine
    pEngine->loadTexture(id, pixels, width, height);

    // free data
    stbi_image_free(pixels);

    Media newImage{};
    newImage.id = id;
    newImage.type = MediaType::Image;
    newImage.filePath = filePath;

    medias.push_back(newImage);
}

void MediaManager::loadVideo(std::string filePath) {
    uint8_t id = newId();

    video = new VulkanVideo(pEngine, filePath);
    
    // TEMP
    pEngine->loadVideoFrame(video->decodeFrame());

    Media newMedia{};
    newMedia.id = id;
    newMedia.type = MediaType::Video;
    newMedia.filePath = filePath;

    medias.push_back(newMedia);
}

void MediaManager::nextFrame() {
    if (video != nullptr) pEngine->loadVideoFrame(video->decodeFrame());
}

std::vector<Media> MediaManager::getMedias() {
    return medias;
}
