#include "../include/media_manager.h"
#include "../include/vk_video.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>

MediaManager::MediaManager(VulkanEngine* pEngine) {
    MediaManager::pEngine = pEngine;
}

void MediaManager::loadImage(std::string filePath) {
    uint8_t newId = 0;

    // find new id
    for (auto image : medias) {
        if (image.id >= newId) {
            newId = image.id + 1;
        }
    }

    // read pixel data
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    // load to engine
    pEngine->loadTexture(newId, pixels, width, height);

    // free data
    stbi_image_free(pixels);

    Media newImage{};
    newImage.id = newId;
    newImage.type = MediaType::Image;
    newImage.filePath = filePath;

    medias.push_back(newImage);
}

void MediaManager::loadVideo(std::string filePath) {
    VulkanVideo* video = new VulkanVideo(pEngine, "video_test.mp4");
    pEngine->loadVideoFrame(video->decodeFrame());
}

std::vector<Media> MediaManager::getMedias() {
    return medias;
}
