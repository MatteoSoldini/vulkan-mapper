#pragma once
#include <string>
#include <vector>
#include <volk.h>
#include "vk_engine.h"

// implementation stolen from: https://wickedengine.net/2023/05/07/vulkan-video-decoding/

#define FRAME_FORMAT VK_FORMAT_G8_B8R8_2PLANE_420_UNORM    //YUV420

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
	VulkanEngine* pDevice;

	uint32_t height;
	uint32_t width;
	uint32_t bitRate;

	// sequence parameter set
	std::vector<uint8_t> spsData;
	uint32_t spsCount = 0;
	std::vector<StdVideoH264SequenceParameterSet> spsArrayH264;

	// picture parameter set
	std::vector<uint8_t> ppsData;
	uint32_t ppsCount;
	std::vector<StdVideoH264PictureParameterSet> ppsArrayH264;

	std::vector<FrameInfo> frameInfos;
	std::vector<uint8_t> frameSliceHeaderData;
	uint32_t framesCount;
	std::vector<size_t> frameDisplayOrder;

	float averageFrameRate;
	float durationSeconds;

	VkBuffer videoStreamBuffer;
	VkDeviceMemory videoStreamBufferMemory;

	// decoded picture buffer (dpb) slots
	uint32_t numDpbSlots = 0;
	std::vector<Texture> decodedPictureBuffer;

	// max number of frame used as reference
	uint32_t numReferenceFrames = 0;

	// video decode
	VkVideoDecodeH264ProfileInfoKHR decodeProfile;
	VkVideoProfileInfoKHR videoProfile;
	VkVideoDecodeH264CapabilitiesKHR h264DecodeCapabilities;
	VkVideoDecodeCapabilitiesKHR decodeCapabilities;
	VkVideoCapabilitiesKHR videoCapabilities;

	int currentFrame = 0;

	// vulkan
	VideoSharedEngineState sharedEngineState;
	VkVideoSessionKHR videoSession = VK_NULL_HANDLE;

	void loadVideo(std::string filePath);
	void queryDecodeVideoCapabilities();
	void loadVideoData();
	void createVideoSession();
	void createTextures();
	void decodeFrame();

public:
	VulkanVideo(VulkanEngine* pDevice, VideoSharedEngineState sharedEngineState, std::string filePath);
};