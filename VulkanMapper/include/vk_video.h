#pragma once
#include <string>
#include <vector>
#include <volk.h>
#include "vk_state.h"
#include "media_manager.h"
#include "video.h"

class VulkanState;

// implementation stolen from: https://wickedengine.net/2023/05/07/vulkan-video-decoding/
#define FRAME_FORMAT VK_FORMAT_G8_B8R8_2PLANE_420_UNORM		//YUV420

struct VmTexture;

struct Video;

typedef struct {
	uint8_t* buffer;
	size_t size;
} INPUT_BUFFER;

struct DecodeFrameResult {
	VkImageView frameImageView;
	VkFence decodeFence;
};

class VulkanVideo {
private:
	VulkanState* pVkState;

	VkCommandBuffer commandBuffer;

	// sequence parameter set
	std::vector<StdVideoH264SequenceParameterSet> spsArrayH264;

	// picture parameter set
	std::vector<StdVideoH264PictureParameterSet> ppsArrayH264;

	// video stream buffer
	VkBuffer videoBitStreamBuffer;
	VkDeviceMemory videoBitStreamBufferMemory;

	// the backing store of DPB slots
	VkImage dpbImage;	// multi layer
	VkDeviceMemory dpbImageMemory;
	VkImageView dpbImageView;
	std::vector<VkImageView> decodedImageViews;

	//uint32_t numReferenceFrames = 0;	// max number of frame used as reference (same as numDpbSlots)

	// profile & capabilities
	VkVideoDecodeH264ProfileInfoKHR decodeProfile;
	VkVideoProfileInfoKHR videoProfile;
	VkVideoDecodeH264CapabilitiesKHR h264DecodeCapabilities;
	VkVideoDecodeCapabilitiesKHR decodeCapabilities;
	VkVideoCapabilitiesKHR videoCapabilities;
	uint64_t bitStreamAlignment;

	// video session
	std::vector<VkDeviceMemory> videoSessionMemories;
	VkVideoSessionParametersKHR videoSessionParameters;
	VkVideoSessionKHR videoSession = VK_NULL_HANDLE;

	// sync
	VkFence decodeFence;

	void loadVideoData(Video* pVideo);
	void createVideoSession(Video* pVideo);
	void createDpbTextures(Video* pVideo);


public:
	VulkanVideo(VulkanState* pDevice);
	~VulkanVideo();
	DecodeFrameResult* decodeFrame(Video* pVideo);
	
	uint64_t queryDecodeVideoCapabilities();
	void loadVideoStream(uint8_t* dataStream, size_t dataStreamSize);
	void setupDecoder(Video* pVideo);
};