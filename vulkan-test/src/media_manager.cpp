#include "../include/media_manager.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>

void MediaManager::loadImage(std::string filePath) {
    uint8_t newId = 0;

    // find new id
    for (auto image : images) {
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

    MediaImage newImage{};
    newImage.id = newId;
    newImage.filePath = filePath;

    images.push_back(newImage);
}

std::vector<MediaImage> MediaManager::getMedias() {
    return images;
}
