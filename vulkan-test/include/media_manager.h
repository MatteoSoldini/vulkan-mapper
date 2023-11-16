#pragma once
#include <string>
#include <vector>
#include "vk_engine.h"
#include "vk_video.h"
#include <chrono>

class VulkanVideo;

typedef uint8_t m_id;

struct DecodeFrameResult;

enum MediaType {
	Image,
	Video,
};

enum FrameType {
	IntraFrame = 0,			// full video frame
	PredictiveFrame = 1,	// contain difference to otherframe
};

struct FrameInfo {
	uint64_t offset = 0;
	uint64_t size = 0;
	float timestampSeconds = 0;
	float durationSeconds = 0;
	FrameType type = FrameType::IntraFrame;
	uint32_t referencePriority = 0;
	int poc = 0;
	int gop = 0;
	int displayOrder = 0;
};

struct VideoState {
	std::string filePath;

	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t bitRate = 0;
	float averageFrameRate = 0.0f;
	float durationSeconds = 0.0f;
	uint64_t bitStreamSize = 0;

	// sequence parameter set
	std::vector<uint8_t> spsData;
	uint32_t spsCount = 0;

	// picture parameter set
	std::vector<uint8_t> ppsData;
	uint32_t ppsCount = 0;

	// frame slice header data
	std::vector<uint8_t> frameSliceHeaderData;
	uint32_t framesCount = 0;

	std::vector<FrameInfo> frameInfos;

	VulkanVideo* pVkDecoder = nullptr;
	// decoded picture buffer
	uint32_t numDpbSlots = 0;

	std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();

	uint64_t currentFrame = 0;
	std::vector<uint8_t> referencesPositions;
	uint8_t currentDecodePosition = 0;
	uint8_t nextDecodePosition = 0;
	DecodeFrameResult* decodingResult = nullptr;
};

struct Media {
	m_id id;
	MediaType type;
	std::string filePath;
	VideoState* pState = nullptr;
};

class MediaManager {
private:
	VulkanEngine* pEngine;
	std::vector<Media> medias;
	m_id newId();

	void startNextFrameDecode();

public:
	MediaManager(VulkanEngine* pEngine);

	// image
	void loadImage(std::string filePath);

	// video
	void loadVideo(std::string filePath);
	
	// video decode loop
	void decodeFrames();

	std::vector<Media> getMedias();
	Media* getMediaById(m_id mediaId);

};