#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

// implementation stolen from: https://wickedengine.net/2023/05/07/vulkan-video-decoding/

typedef struct {
	uint8_t* buffer;
	size_t size;
} INPUT_BUFFER;

enum FrameType {
	IntraFrame = 0,			// full video frame
	PredictiveFrame = 1,	// contain difference to otherframe
};

struct VideoSharedEngineState {
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	uint32_t videoFamily;
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

class VulkanVideo {
private:
	uint32_t height;
	uint32_t width;
	uint32_t bitRate;

	std::vector<uint8_t> spsData;
	uint32_t spsCount = 0;
	std::vector<uint8_t> ppsData;
	uint32_t ppsCount;

	std::vector<FrameInfo> frameInfos;
	std::vector<uint8_t> frameSliceHeaderData;
	uint32_t framesCount;
	std::vector<size_t> frameDisplayOrder;

	float averageFrameRate;
	float durationSeconds;

	// decoded picture buffer (dpb) slots
	uint32_t numDpbSlots = 0;

	int currentFrame = 0;
	
	// vulkan
	VideoSharedEngineState sharedEngineState;
	VkVideoSessionKHR videoSession;

	void initDecoder();
	void createVideoSession();
	void loadVideo(std::string filePath);

public:
	VulkanVideo(VideoSharedEngineState sharedEngineState, std::string filePath);
};