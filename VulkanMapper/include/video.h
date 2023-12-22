#pragma once

#include <vector>
#include <chrono>
#include <string>

#include "vk_video.h"
#include "vk_state.h"
#include "vm_types.h"
#include "media.h"

class VulkanVideo;

struct DecodeFrameResult;

class VulkanState;

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

class Video : public Media {
private:
	VulkanState* pDevice;
	VmVideoFrameStreamId_t vmVideoFrameStreamId;
	bool presentAFrame = true; // emit first frame anyways

public:
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

	std::vector<FrameInfo> frameInfos;

	VulkanVideo* pVkDecoder = nullptr;
	// decoded picture buffer
	uint32_t numDpbSlots = 0;

	std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();

	std::vector<uint8_t> referencesPositions;
	uint8_t currentDecodePosition = 0;
	uint8_t nextDecodePosition = 0;

	Video(MediaId_t id, VulkanState* pDevice, std::string filePath);

	uint64_t currentFrame = 0;
	uint32_t framesCount = 0;
	
	void decodeFrame();
	DecodeFrameResult* decodingResult = nullptr;

	bool playing = false;	// default: not playing
	void pause();
	void play();
	void firstFrame();

	VmVideoFrameStreamId_t getVmVideoFrameStreamId() {
		return vmVideoFrameStreamId;
	}
};