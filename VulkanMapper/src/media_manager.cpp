#include "../include/media_manager.h"
//#include "../include/vk_video.h"
#include "../include/read_file.h"


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>
#include <minimp4.h>
#include <iostream>
#include <algorithm>


MediaManager::MediaManager(VulkanEngine* pEngine) {
    MediaManager::pEngine = pEngine;
}

void MediaManager::loadFile(std::string filePath) {
    std::string fileExtension = filePath.substr(filePath.find_last_of(".") + 1);

    if (fileExtension == "mp4") {
        loadVideo(filePath);
    }
    else if (fileExtension == "jpg" || fileExtension == "png") {
        loadImage(filePath);
    }
}

m_id MediaManager::newId() {
    m_id newId = 0;

    // find new id
    for (auto media : medias) {
        if (media.id >= newId) {
            newId = media.id + 1;
        }
    }

    return newId;
}


void MediaManager::loadImage(std::string filePath) {
    m_id id = newId();
    
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
    newImage.type = MediaType::IMAGE;
    newImage.filePath = filePath;

    medias.push_back(newImage);
}

void MediaManager::decodeFrames() {
    // check for any video that need decoding
    for (auto media : medias) {
        if (media.type == MediaType::VIDEO) {   // VIDEO
            Video* pVideo = dynamic_cast<Video*>(media.pState);
            if (pVideo == nullptr) throw std::runtime_error("wrong media state");

            pVideo->decodeFrame();
        }
    }
}

std::vector<Media> MediaManager::getMedias() {
    return medias;
}

Media* MediaManager::getMediaById(m_id mediaId) {
    for (int i = 0; i < medias.size(); i++) {
        if (medias[i].id == mediaId) {
            return &medias[i];
        }
    }

    return nullptr;
}

void MediaManager::loadVideo(std::string filePath) {
    // add video to media
    m_id id = newId();

    Media media{};
    media.id = id;
    media.type = MediaType::VIDEO;
    media.filePath = filePath;
    media.pState = new Video(pEngine, filePath);
    medias.push_back(media);
}