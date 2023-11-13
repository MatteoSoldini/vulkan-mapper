#pragma once
#include <string>
#include <vector>
#include <volk.h>
#include "vk_engine.h"

class VulkanEngine;

// implementation stolen from: https://wickedengine.net/2023/05/07/vulkan-video-decoding/
#define FRAME_FORMAT VK_FORMAT_G8_B8R8_2PLANE_420_UNORM		//YUV420

struct Texture;

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
	VulkanEngine* pDevice;

	// sequence parameter set
	std::vector<StdVideoH264SequenceParameterSet> spsArrayH264;

	// picture parameter set
	std::vector<StdVideoH264PictureParameterSet> ppsArrayH264;

	// video stream buffer
	VkBuffer videoBitStreamBuffer;
	VkDeviceMemory videoBitStreamBufferMemory;

	// frames used to generate p-frames, also known as decoded picture buffer (dpb) slots
	std::vector<uint8_t> referencePositions;	// positions of the reference frames (slots) inside the dpb
	uint8_t nextDecodePosition = 0;
	uint8_t currentDecodePosition = 0;

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

	int currentFrame = 0;

	// decoder -> CHECKME: consider moving to vulkan device class
	VkVideoSessionParametersKHR videoSessionParameters;
	VkVideoSessionKHR videoSession = VK_NULL_HANDLE;

	// sync
	VkFence decodeFence;



public:
	VulkanVideo(VulkanEngine* pDevice, std::string filePath);
	DecodeFrameResult* startNextFrameDecode();
	
	uint64_t queryDecodeVideoCapabilities();
	void loadVideoTrack(VideoState* pVideoState);
	void loadVideoData();
	void createVideoSession();
	void createDpbTextures();
};