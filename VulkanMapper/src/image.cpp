#include "../include/image.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Image::Image(MediaId_t id, VulkanState* pDevice, std::string filePath) : Media(id, filePath) {
    
    Image::pDevice = pDevice;

    int width, height, channels;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    // load to engine
    textureId = pDevice->loadTexture(pixels, width, height);

    // free data
    stbi_image_free(pixels);
}
