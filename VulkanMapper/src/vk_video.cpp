#include <iostream>

#include "../include/vk_video.h"
#include "../include/read_file.h"

#define MINIMP4_IMPLEMENTATION
#ifdef _WIN32
#include <sys/types.h>
#include <stddef.h>
typedef size_t ssize_t;
#endif
#include <minimp4.h>

#define H264_IMPLEMENTATION
#include <h264.h>
#include <algorithm>

#include "../include/vk_utils.h"

void VulkanVideo::createVideoSession(Video* pVideoState) {
    // create command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pVkState->getVideoCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(pVkState->getDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // create fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    //fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(pVkState->getDevice(), &fenceInfo, nullptr, &decodeFence);

    // create video session
    VkVideoSessionCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
    info.queueFamilyIndex = pVkState->getVideoQueueFamilyIndex();
    info.maxActiveReferencePictures = pVideoState->numDpbSlots; //numReferenceFrames;
    info.maxDpbSlots = pVideoState->numDpbSlots;
    info.maxCodedExtent.width = std::min(pVideoState->width, videoCapabilities.maxCodedExtent.width);
    info.maxCodedExtent.height = std::min(pVideoState->height, videoCapabilities.maxCodedExtent.height);
    info.pictureFormat = FRAME_FORMAT;
    info.referencePictureFormat = info.pictureFormat;
    info.pVideoProfile = &videoProfile;
    info.pStdHeaderVersion = &videoCapabilities.stdHeaderVersion;

    if (vkCreateVideoSessionKHR(pVkState->getDevice(), &info, nullptr, &videoSession) != VK_SUCCESS) {
        throw std::runtime_error("failed to create video session");
    }

    // query memory requirements
    uint32_t requirementsCount = 0;
    vkGetVideoSessionMemoryRequirementsKHR(pVkState->getDevice(), videoSession, &requirementsCount, nullptr);
    std::vector<VkVideoSessionMemoryRequirementsKHR> videoSessionRequirements(requirementsCount);

    for (auto& videoSessionRequirement : videoSessionRequirements) {
        videoSessionRequirement.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
    }

    if (vkGetVideoSessionMemoryRequirementsKHR(pVkState->getDevice(), videoSession, &requirementsCount, videoSessionRequirements.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to get video session memory requirements");
    }

    // allocate and bind memory
    //internal_state->allocations.resize(requirement_count);
    videoSessionMemories.resize(requirementsCount);
    for (uint32_t i = 0; i < requirementsCount; ++i) {
        VkMemoryRequirements memoryRequirements = videoSessionRequirements[i].memoryRequirements;
        
        // allocate memory
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;

        allocInfo.memoryTypeIndex = findGenericMemoryType(
            pVkState->getPhysicalDevice(),
            memoryRequirements.memoryTypeBits
        );

        if (vkAllocateMemory(pVkState->getDevice(), &allocInfo, nullptr, &videoSessionMemories[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate memory");
        }

        // bind memory
        VkBindVideoSessionMemoryInfoKHR bindInfo = {};
        bindInfo.sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
        bindInfo.memory = videoSessionMemories[i];
        bindInfo.memoryBindIndex = videoSessionRequirements[i].memoryBindIndex;
        bindInfo.memoryOffset = 0;
        bindInfo.memorySize = allocInfo.allocationSize;

        if (vkBindVideoSessionMemoryKHR(pVkState->getDevice(), videoSession, 1, &bindInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to bind video session memory");
        }
    }

    // create session parameters
    VkVideoDecodeH264SessionParametersAddInfoKHR sessionParametersAddInfoH264 = {};
    sessionParametersAddInfoH264.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR;
    sessionParametersAddInfoH264.stdPPSCount = pVideoState->ppsCount;
    sessionParametersAddInfoH264.pStdPPSs = ppsArrayH264.data();
    sessionParametersAddInfoH264.stdSPSCount = pVideoState->spsCount;
    sessionParametersAddInfoH264.pStdSPSs = spsArrayH264.data();
    
    VkVideoDecodeH264SessionParametersCreateInfoKHR sessionParametersInfoH264 = {};
    sessionParametersInfoH264.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR;
    sessionParametersInfoH264.maxStdPPSCount = pVideoState->ppsCount;
    sessionParametersInfoH264.maxStdSPSCount = pVideoState->spsCount;
    sessionParametersInfoH264.pParametersAddInfo = &sessionParametersAddInfoH264;

    VkVideoSessionParametersCreateInfoKHR sessionParametersInfo = {};
    sessionParametersInfo.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;
    sessionParametersInfo.videoSession = videoSession;
    sessionParametersInfo.videoSessionParametersTemplate = VK_NULL_HANDLE;
    sessionParametersInfo.pNext = &sessionParametersInfoH264;

    if (vkCreateVideoSessionParametersKHR(pVkState->getDevice(), &sessionParametersInfo, nullptr, &videoSessionParameters) != VK_SUCCESS) {
        throw std::runtime_error("failed to create video session parameters");
    }
}

void VulkanVideo::createDpbTextures(Video* pVideoState) {
    // create image
    VkVideoProfileListInfoKHR profileListInfo = {};
    profileListInfo.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
    profileListInfo.pProfiles = &videoProfile;
    profileListInfo.profileCount = 1;

    VkImageUsageFlags usageFlags = 0;
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usageFlags |= VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
    usageFlags |= VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
    usageFlags |= VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;


    pVkState->createImage(
        pVideoState->width,
        pVideoState->height,
        pVideoState->numDpbSlots,
        FRAME_FORMAT,
        VK_IMAGE_TILING_OPTIMAL,
        usageFlags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        dpbImage,
        dpbImageMemory,
        &profileListInfo
    );

    pVkState->transitionImageLayout(dpbImage, FRAME_FORMAT, pVideoState->numDpbSlots, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // dpb image view
    VkSamplerYcbcrConversionInfo conversionInfo = {};
    conversionInfo.conversion = pVkState->getYcbcrSamplerConversion();
    conversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR;
    conversionInfo.pNext = nullptr;

    // image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = dpbImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = FRAME_FORMAT;

    // subresourceRange describes what the image's purposvie is and which part of the image should be accessed
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;             // the first mipmap level accessible to the view
    viewInfo.subresourceRange.levelCount = 1;               // the number of mipmap levels (starting from baseMipLevel) accessible to the view
    viewInfo.subresourceRange.baseArrayLayer = 0;           // the first array layer accessible to the view
    viewInfo.subresourceRange.layerCount = pVideoState->numDpbSlots;     // the number of array layers (starting from baseArrayLayer) accessible to the view
    viewInfo.pNext = &conversionInfo;

    if (vkCreateImageView(pVkState->getDevice(), &viewInfo, nullptr, &dpbImageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    // decoded image view
    decodedImageViews.resize(pVideoState->numDpbSlots);    // resize image view
    for (int i = 0; i < pVideoState->numDpbSlots; i++) {
        VkSamplerYcbcrConversionInfo conversionInfo = {};
        conversionInfo.conversion = pVkState->getYcbcrSamplerConversion();
        conversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR;
        conversionInfo.pNext = nullptr;

        // image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = dpbImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = FRAME_FORMAT;

        // subresourceRange describes what the image's purpose is and which part of the image should be accessed
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;         // the first mipmap level accessible to the view
        viewInfo.subresourceRange.levelCount = 1;           // the number of mipmap levels (starting from baseMipLevel) accessible to the view
        viewInfo.subresourceRange.baseArrayLayer = i;       // the first array layer accessible to the view
        viewInfo.subresourceRange.layerCount = 1;           // the number of array layers (starting from baseArrayLayer) accessible to the view
        viewInfo.pNext = &conversionInfo;

        if (vkCreateImageView(pVkState->getDevice(), &viewInfo, nullptr, &decodedImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
    }
}

DecodeFrameResult* VulkanVideo::decodeFrame(Video* pVideoState) {
    const FrameInfo currentFrameInfo = pVideoState->frameInfos[pVideoState->currentFrame];

    // TODO: ensure that referenced dpb slots are in SRC state
    const h264::SliceHeader* frameSliceHeader = (const h264::SliceHeader*)pVideoState->frameSliceHeaderData.data() + pVideoState->currentFrame;
    const h264::PPS* pps = (const h264::PPS*)pVideoState->ppsData.data() + frameSliceHeader->pic_parameter_set_id;
    const h264::SPS* sps = (const h264::SPS*)pVideoState->spsData.data() + pps->seq_parameter_set_id;

    // (reference) slots info
    std::vector<VkVideoReferenceSlotInfoKHR> referenceSlotInfos;
    int refSlotPositionsCount = pVideoState->referencesPositions.size();
    referenceSlotInfos.resize(refSlotPositionsCount + 1);
    
    // set reference slots
    for (size_t i = 0; i < refSlotPositionsCount; i++) {
        uint32_t refSlotPosition = pVideoState->referencesPositions[i];
        assert(refSlotPosition != pVideoState->currentDecodePosition);  // decode slot should not be overwritten by a reference frame

        VkVideoPictureResourceInfoKHR picRefSlotInfo = {};
        picRefSlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
        picRefSlotInfo.codedOffset.x = 0;
        picRefSlotInfo.codedOffset.y = 0;
        picRefSlotInfo.codedExtent.width = pVideoState->width;
        picRefSlotInfo.codedExtent.height = pVideoState->height;
        picRefSlotInfo.baseArrayLayer = refSlotPosition; // i
        picRefSlotInfo.imageViewBinding = dpbImageView;

        StdVideoDecodeH264ReferenceInfo h264RefInfo = {};
        h264RefInfo.flags.bottom_field_flag = 0;
        h264RefInfo.flags.top_field_flag = 0;
        h264RefInfo.flags.is_non_existing = 0;
        h264RefInfo.flags.used_for_long_term_reference = 0;
        h264RefInfo.FrameNum = frameSliceHeader->frame_num;
        h264RefInfo.PicOrderCnt[0] = currentFrameInfo.poc;
        h264RefInfo.PicOrderCnt[1] = currentFrameInfo.poc;

        VkVideoDecodeH264DpbSlotInfoKHR h264SlotInfo = {};
        h264SlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
        h264SlotInfo.pStdReferenceInfo = &h264RefInfo;

        VkVideoReferenceSlotInfoKHR refSlotInfo = {};
        refSlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
        refSlotInfo.pPictureResource = &picRefSlotInfo;
        refSlotInfo.slotIndex = refSlotPosition;
        refSlotInfo.pNext = &h264SlotInfo;
        
        // set slot
        referenceSlotInfos[i] = refSlotInfo;
    }

    // create decode slot
    VkVideoPictureResourceInfoKHR picRefSlotInfo = {};
    picRefSlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
    picRefSlotInfo.codedOffset.x = 0;
    picRefSlotInfo.codedOffset.y = 0;
    picRefSlotInfo.codedExtent.width = pVideoState->width;
    picRefSlotInfo.codedExtent.height = pVideoState->height;
    picRefSlotInfo.baseArrayLayer = pVideoState->currentDecodePosition; // i
    picRefSlotInfo.imageViewBinding = dpbImageView;

    StdVideoDecodeH264ReferenceInfo h264RefInfo = {};
    h264RefInfo.flags.bottom_field_flag = 0;
    h264RefInfo.flags.top_field_flag = 0;
    h264RefInfo.flags.is_non_existing = 0;
    h264RefInfo.flags.used_for_long_term_reference = 0;
    h264RefInfo.FrameNum = frameSliceHeader->frame_num;
    h264RefInfo.PicOrderCnt[0] = currentFrameInfo.poc;
    h264RefInfo.PicOrderCnt[1] = currentFrameInfo.poc;

    VkVideoDecodeH264DpbSlotInfoKHR h264SlotInfo = {};
    h264SlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
    h264SlotInfo.pStdReferenceInfo = &h264RefInfo;

    VkVideoReferenceSlotInfoKHR dstSlotInfo = {};
    dstSlotInfo.sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
    dstSlotInfo.pPictureResource = &picRefSlotInfo;
    dstSlotInfo.slotIndex = pVideoState->currentDecodePosition;
    dstSlotInfo.pNext = &h264SlotInfo;

    referenceSlotInfos[refSlotPositionsCount] = dstSlotInfo;
    referenceSlotInfos[refSlotPositionsCount].slotIndex = -1; // target of picture reconstruction
    
    // stuff
    StdVideoDecodeH264PictureInfo stdPictureInfoH264 = {};
    stdPictureInfoH264.pic_parameter_set_id = frameSliceHeader->pic_parameter_set_id;
    stdPictureInfoH264.seq_parameter_set_id = pps->seq_parameter_set_id;
    stdPictureInfoH264.frame_num = frameSliceHeader->frame_num;
    stdPictureInfoH264.PicOrderCnt[0] = currentFrameInfo.poc;
    stdPictureInfoH264.PicOrderCnt[1] = currentFrameInfo.poc;
    stdPictureInfoH264.idr_pic_id = frameSliceHeader->idr_pic_id;
    stdPictureInfoH264.flags.is_intra = currentFrameInfo.type == FrameType::IntraFrame ? 1 : 0;
    stdPictureInfoH264.flags.is_reference = currentFrameInfo.referencePriority > 0 ? 1 : 0;
    stdPictureInfoH264.flags.IdrPicFlag = (stdPictureInfoH264.flags.is_intra && stdPictureInfoH264.flags.is_reference) ? 1 : 0;
    stdPictureInfoH264.flags.field_pic_flag = frameSliceHeader->field_pic_flag;
    stdPictureInfoH264.flags.bottom_field_flag = frameSliceHeader->bottom_field_flag;
    stdPictureInfoH264.flags.complementary_field_pair = 0;

    uint32_t sliceOffset = 0;

    VkVideoDecodeH264PictureInfoKHR pictureInfoH264 = {};
    pictureInfoH264.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR;
    pictureInfoH264.pStdPictureInfo = &stdPictureInfoH264;
    pictureInfoH264.sliceCount = 1;
    pictureInfoH264.pSliceOffsets = &sliceOffset;

    // begin decode
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // begin coding
    // include all slots
    VkVideoBeginCodingInfoKHR videoBeginInfo = {};
    videoBeginInfo.sType = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR;
    videoBeginInfo.videoSession = videoSession;
    videoBeginInfo.videoSessionParameters = videoSessionParameters;
    videoBeginInfo.referenceSlotCount = refSlotPositionsCount + 1; // add in the current reconstructed DPB image
    videoBeginInfo.pReferenceSlots = videoBeginInfo.referenceSlotCount == 0 ? nullptr : referenceSlotInfos.data();
    
    vkCmdBeginVideoCodingKHR(commandBuffer, &videoBeginInfo);

    // reset decode on first frame
    if (pVideoState->currentFrame == 0) {
        VkVideoCodingControlInfoKHR controlInfo = {};
        controlInfo.sType = VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR;
        controlInfo.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR;
        vkCmdControlVideoCodingKHR(commandBuffer, &controlInfo);
    }

    // include only the slots that are used as reference
    VkVideoDecodeInfoKHR decodeInfo = {};
    decodeInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR;
    decodeInfo.srcBuffer = videoBitStreamBuffer;
    decodeInfo.srcBufferOffset = (VkDeviceSize)currentFrameInfo.offset;
    decodeInfo.srcBufferRange = (VkDeviceSize)((currentFrameInfo.size + bitStreamAlignment - 1) / bitStreamAlignment) * bitStreamAlignment;
    decodeInfo.dstPictureResource = *dstSlotInfo.pPictureResource;
    decodeInfo.referenceSlotCount = refSlotPositionsCount;
    decodeInfo.pReferenceSlots = decodeInfo.referenceSlotCount == 0 ? nullptr : referenceSlotInfos.data();
    decodeInfo.pSetupReferenceSlot = &dstSlotInfo;
    decodeInfo.pNext = &pictureInfoH264;

    vkCmdDecodeVideoKHR(commandBuffer, &decodeInfo);

    // end decode
    VkVideoEndCodingInfoKHR endInfo = {};
    endInfo.sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR;
    vkCmdEndVideoCodingKHR(commandBuffer, &endInfo);


    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to end recording command buffer");
    }
    
    // submit command buffer
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(pVkState->getVideoQueue(), 1, &submitInfo, decodeFence);

    return new DecodeFrameResult{ decodedImageViews[pVideoState->currentDecodePosition], decodeFence };
}

VulkanVideo::VulkanVideo(VulkanState* pDevice) {
    VulkanVideo::pVkState = pDevice;
}

VulkanVideo::~VulkanVideo() {
    vkDeviceWaitIdle(pVkState->getDevice());
    // destroy bitstream buffer
    vkDestroyBuffer(pVkState->getDevice(), videoBitStreamBuffer, nullptr);
    vkFreeMemory(pVkState->getDevice(), videoBitStreamBufferMemory, nullptr);

    // destroy dpb
    vkDestroyImageView(pVkState->getDevice(), dpbImageView, nullptr);
    vkDestroyImage(pVkState->getDevice(), dpbImage, nullptr);
    vkFreeMemory(pVkState->getDevice(), dpbImageMemory, nullptr);
    for (auto decodedImageView : decodedImageViews) {
        vkDestroyImageView(pVkState->getDevice(), decodedImageView, nullptr);
    }

    // destroy decode fence
    vkDestroyFence(pVkState->getDevice(), decodeFence, nullptr);

    // destroy video session
    vkDestroyVideoSessionParametersKHR(pVkState->getDevice(), videoSessionParameters, nullptr);
    vkDestroyVideoSessionKHR(pVkState->getDevice(), videoSession, nullptr);
    for (auto videoSessionMemory : videoSessionMemories) {
        vkFreeMemory(pVkState->getDevice(), videoSessionMemory, nullptr);
    }
}

uint64_t VulkanVideo::queryDecodeVideoCapabilities() {
    decodeProfile = {};
    decodeProfile.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR;
    decodeProfile.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_HIGH;
    decodeProfile.pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR;

    videoProfile = {};
    videoProfile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
    videoProfile.pNext = &decodeProfile;
    videoProfile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
    videoProfile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    videoProfile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    videoProfile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;

    h264DecodeCapabilities = {};
    h264DecodeCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR;

    decodeCapabilities = {};
    decodeCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
    decodeCapabilities.pNext = &h264DecodeCapabilities;

    videoCapabilities = {};
    videoCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
    videoCapabilities.pNext = &decodeCapabilities;

    if (vkGetPhysicalDeviceVideoCapabilitiesKHR(pVkState->getPhysicalDevice(), &videoProfile, &videoCapabilities) != VK_SUCCESS) {
        throw std::runtime_error("failed to get device video capabilities");
    }

    bitStreamAlignment = std::max(
        videoCapabilities.minBitstreamBufferOffsetAlignment, videoCapabilities.minBitstreamBufferSizeAlignment
    );
    return bitStreamAlignment;
}

void VulkanVideo::loadVideoStream(uint8_t* dataStream, size_t dataStreamSize) {
    // FIX: redundant info: dataStreamSize & bufferSize
    
    // copy video track to gpu
    VkDeviceSize bufferSize = dataStreamSize;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    pVkState->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, nullptr);

    VkDevice device = pVkState->getDevice();

    // copy stream data
    void* streamData;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &streamData);
    
    std::memcpy(streamData, dataStream, dataStreamSize);

    vkUnmapMemory(device, stagingBufferMemory);

    VkVideoProfileListInfoKHR profileList = {};
    profileList.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
    profileList.profileCount = 1;
    profileList.pProfiles = &videoProfile;

    pVkState->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        videoBitStreamBuffer,
        videoBitStreamBufferMemory,
        &profileList
    );

    pVkState->copyBuffer(stagingBuffer, videoBitStreamBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanVideo::setupDecoder(Video* pVideoState) {
    loadVideoData(pVideoState);
    createVideoSession(pVideoState);
    createDpbTextures(pVideoState);
}

void VulkanVideo::loadVideoData(Video* pVideoState) {
    // load pps array
    ppsArrayH264.resize(pVideoState->ppsCount);
    std::vector<StdVideoH264ScalingLists> scalinglistArrayH264(pVideoState->ppsCount);

    for (int i = 0; i < pVideoState->ppsCount; i++) {
        const h264::PPS* pps = (const h264::PPS*)pVideoState->ppsData.data() + i;
        StdVideoH264PictureParameterSet& vkPps = ppsArrayH264[i];
        StdVideoH264ScalingLists& vkScalinglist = scalinglistArrayH264[i];

        vkPps.flags.transform_8x8_mode_flag = pps->transform_8x8_mode_flag;
        vkPps.flags.redundant_pic_cnt_present_flag = pps->redundant_pic_cnt_present_flag;
        vkPps.flags.constrained_intra_pred_flag = pps->constrained_intra_pred_flag;
        vkPps.flags.deblocking_filter_control_present_flag = pps->deblocking_filter_control_present_flag;
        vkPps.flags.weighted_pred_flag = pps->weighted_pred_flag;
        vkPps.flags.bottom_field_pic_order_in_frame_present_flag = pps->pic_order_present_flag;
        vkPps.flags.entropy_coding_mode_flag = pps->entropy_coding_mode_flag;
        vkPps.flags.pic_scaling_matrix_present_flag = pps->pic_scaling_matrix_present_flag;

        vkPps.seq_parameter_set_id = pps->seq_parameter_set_id;
        vkPps.pic_parameter_set_id = pps->pic_parameter_set_id;
        vkPps.num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_active_minus1;
        vkPps.num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_active_minus1;
        vkPps.weighted_bipred_idc = (StdVideoH264WeightedBipredIdc)pps->weighted_bipred_idc;
        vkPps.pic_init_qp_minus26 = pps->pic_init_qp_minus26;
        vkPps.pic_init_qs_minus26 = pps->pic_init_qs_minus26;
        vkPps.chroma_qp_index_offset = pps->chroma_qp_index_offset;
        vkPps.second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;

        vkPps.pScalingLists = &vkScalinglist;

        int count = sizeof(pps->pic_scaling_list_present_flag) / sizeof(pps->pic_scaling_list_present_flag[0]);
        for (int j = 0; j < count; ++j) {
            vkScalinglist.scaling_list_present_mask |= pps->pic_scaling_list_present_flag[j] << j;
        }

        count = sizeof(pps->UseDefaultScalingMatrix4x4Flag) / sizeof(pps->UseDefaultScalingMatrix4x4Flag[0]);
        for (int j = 0; j < count; ++j) {
            vkScalinglist.use_default_scaling_matrix_mask |= pps->UseDefaultScalingMatrix4x4Flag[j] << j;
        }

        count = sizeof(pps->ScalingList4x4) / sizeof(pps->ScalingList4x4[0]);
        for (int j = 0; j < count; ++j) {
            for (int k = 0; k < count; ++k) {
                vkScalinglist.ScalingList4x4[j][k] = (uint8_t)pps->ScalingList4x4[j][k];
            }
        }

        count = sizeof(pps->ScalingList8x8) / sizeof(pps->ScalingList8x8[0]);
        for (int j = 0; j < count; ++j) {
            for (int k = 0; k < count; ++k) {
                vkScalinglist.ScalingList8x8[j][k] = (uint8_t)pps->ScalingList8x8[j][k];
            }
        }
    }

    // load sps array
    spsArrayH264.resize(pVideoState->spsCount);
    std::vector<StdVideoH264SequenceParameterSetVui> vuiArrayH264(pVideoState->spsCount);
    std::vector<StdVideoH264HrdParameters> hrdArrayH264(pVideoState->spsCount);
    
    uint32_t numReferenceFrames = 0;

    for (uint32_t i = 0; i < pVideoState->spsCount; ++i) {
        const h264::SPS* sps = (const h264::SPS*)pVideoState->spsData.data() + i;
        StdVideoH264SequenceParameterSet& vkSps = spsArrayH264[i];

        vkSps.flags.constraint_set0_flag = sps->constraint_set0_flag;
        vkSps.flags.constraint_set1_flag = sps->constraint_set1_flag;
        vkSps.flags.constraint_set2_flag = sps->constraint_set2_flag;
        vkSps.flags.constraint_set3_flag = sps->constraint_set3_flag;
        vkSps.flags.constraint_set4_flag = sps->constraint_set4_flag;
        vkSps.flags.constraint_set5_flag = sps->constraint_set5_flag;
        vkSps.flags.direct_8x8_inference_flag = sps->direct_8x8_inference_flag;
        vkSps.flags.mb_adaptive_frame_field_flag = sps->mb_adaptive_frame_field_flag;
        vkSps.flags.frame_mbs_only_flag = sps->frame_mbs_only_flag;
        vkSps.flags.delta_pic_order_always_zero_flag = sps->delta_pic_order_always_zero_flag;
        vkSps.flags.separate_colour_plane_flag = sps->separate_colour_plane_flag;
        vkSps.flags.gaps_in_frame_num_value_allowed_flag = sps->gaps_in_frame_num_value_allowed_flag;
        vkSps.flags.qpprime_y_zero_transform_bypass_flag = sps->qpprime_y_zero_transform_bypass_flag;
        vkSps.flags.frame_cropping_flag = sps->frame_cropping_flag;
        vkSps.flags.seq_scaling_matrix_present_flag = sps->seq_scaling_matrix_present_flag;
        vkSps.flags.vui_parameters_present_flag = sps->vui_parameters_present_flag;

        if (vkSps.flags.vui_parameters_present_flag) {
            StdVideoH264SequenceParameterSetVui& vkVui = vuiArrayH264[i];
            vkSps.pSequenceParameterSetVui = &vkVui;
            vkVui.flags.aspect_ratio_info_present_flag = sps->vui.aspect_ratio_info_present_flag;
            vkVui.flags.overscan_info_present_flag = sps->vui.overscan_info_present_flag;
            vkVui.flags.overscan_appropriate_flag = sps->vui.overscan_appropriate_flag;
            vkVui.flags.video_signal_type_present_flag = sps->vui.video_signal_type_present_flag;
            vkVui.flags.video_full_range_flag = sps->vui.video_full_range_flag;
            vkVui.flags.color_description_present_flag = sps->vui.colour_description_present_flag;
            vkVui.flags.chroma_loc_info_present_flag = sps->vui.chroma_loc_info_present_flag;
            vkVui.flags.timing_info_present_flag = sps->vui.timing_info_present_flag;
            vkVui.flags.fixed_frame_rate_flag = sps->vui.fixed_frame_rate_flag;
            vkVui.flags.bitstream_restriction_flag = sps->vui.bitstream_restriction_flag;
            vkVui.flags.nal_hrd_parameters_present_flag = sps->vui.nal_hrd_parameters_present_flag;
            vkVui.flags.vcl_hrd_parameters_present_flag = sps->vui.vcl_hrd_parameters_present_flag;

            vkVui.aspect_ratio_idc = (StdVideoH264AspectRatioIdc)sps->vui.aspect_ratio_idc;
            vkVui.sar_width = sps->vui.sar_width;
            vkVui.sar_height = sps->vui.sar_height;
            vkVui.video_format = sps->vui.video_format;
            vkVui.colour_primaries = sps->vui.colour_primaries;
            vkVui.transfer_characteristics = sps->vui.transfer_characteristics;
            vkVui.matrix_coefficients = sps->vui.matrix_coefficients;
            vkVui.num_units_in_tick = sps->vui.num_units_in_tick;
            vkVui.time_scale = sps->vui.time_scale;
            vkVui.max_num_reorder_frames = sps->vui.num_reorder_frames;
            vkVui.max_dec_frame_buffering = sps->vui.max_dec_frame_buffering;
            vkVui.chroma_sample_loc_type_top_field = sps->vui.chroma_sample_loc_type_top_field;
            vkVui.chroma_sample_loc_type_bottom_field = sps->vui.chroma_sample_loc_type_bottom_field;

            StdVideoH264HrdParameters& vkHrd = hrdArrayH264[i];
            vkVui.pHrdParameters = &vkHrd;
            vkHrd.cpb_cnt_minus1 = sps->hrd.cpb_cnt_minus1;
            vkHrd.bit_rate_scale = sps->hrd.bit_rate_scale;
            vkHrd.cpb_size_scale = sps->hrd.cpb_size_scale;

            int count = sizeof(sps->hrd.bit_rate_value_minus1) / sizeof(sps->hrd.bit_rate_value_minus1[0]);
            for (int j = 0; j < count; ++j) {
                vkHrd.bit_rate_value_minus1[j] = sps->hrd.bit_rate_value_minus1[j];
                vkHrd.cpb_size_value_minus1[j] = sps->hrd.cpb_size_value_minus1[j];
                vkHrd.cbr_flag[j] = sps->hrd.cbr_flag[j];
            }
            vkHrd.initial_cpb_removal_delay_length_minus1 = sps->hrd.initial_cpb_removal_delay_length_minus1;
            vkHrd.cpb_removal_delay_length_minus1 = sps->hrd.cpb_removal_delay_length_minus1;
            vkHrd.dpb_output_delay_length_minus1 = sps->hrd.dpb_output_delay_length_minus1;
            vkHrd.time_offset_length = sps->hrd.time_offset_length;
        }

        vkSps.profile_idc = (StdVideoH264ProfileIdc)sps->profile_idc;
        switch (sps->level_idc) {
        case 0:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_1_0;
            break;
        case 11:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_1_1;
            break;
        case 12:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_1_2;
            break;
        case 13:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_1_3;
            break;
        case 20:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_2_0;
            break;
        case 21:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_2_1;
            break;
        case 22:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_2_2;
            break;
        case 30:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_3_0;
            break;
        case 31:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_3_1;
            break;
        case 32:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_3_2;
            break;
        case 40:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_4_0;
            break;
        case 41:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_4_1;
            break;
        case 42:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_4_2;
            break;
        case 50:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_5_0;
            break;
        case 51:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_5_1;
            break;
        case 52:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_5_2;
            break;
        case 60:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_6_0;
            break;
        case 61:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_6_1;
            break;
        case 62:
            vkSps.level_idc = STD_VIDEO_H264_LEVEL_IDC_6_2;
            break;
        default:
            assert(0);
            break;
        }

        assert(vkSps.level_idc <= h264DecodeCapabilities.maxLevelIdc);
        //vk_sps.chroma_format_idc = (StdVideoH264ChromaFormatIdc)sps->chroma_format_idc;
        vkSps.chroma_format_idc = STD_VIDEO_H264_CHROMA_FORMAT_IDC_420; // only one we support currently
        vkSps.seq_parameter_set_id = sps->seq_parameter_set_id;
        vkSps.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
        vkSps.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
        vkSps.log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
        vkSps.pic_order_cnt_type = (StdVideoH264PocType)sps->pic_order_cnt_type;
        vkSps.offset_for_non_ref_pic = sps->offset_for_non_ref_pic;
        vkSps.offset_for_top_to_bottom_field = sps->offset_for_top_to_bottom_field;
        vkSps.log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4;
        vkSps.num_ref_frames_in_pic_order_cnt_cycle = sps->num_ref_frames_in_pic_order_cnt_cycle;
        vkSps.max_num_ref_frames = sps->num_ref_frames;
        vkSps.pic_width_in_mbs_minus1 = sps->pic_width_in_mbs_minus1;
        vkSps.pic_height_in_map_units_minus1 = sps->pic_height_in_map_units_minus1;
        vkSps.frame_crop_left_offset = sps->frame_crop_left_offset;
        vkSps.frame_crop_right_offset = sps->frame_crop_right_offset;
        vkSps.frame_crop_top_offset = sps->frame_crop_top_offset;
        vkSps.frame_crop_bottom_offset = sps->frame_crop_bottom_offset;
        vkSps.pOffsetForRefFrame = sps->offset_for_ref_frame;

        numReferenceFrames = std::max(numReferenceFrames, (uint32_t)sps->num_ref_frames);
    }

    // *2: top and bottom field counts as two I think: https://vulkan.lunarg.com/doc/view/1.3.239.0/windows/1.3-extensions/vkspec.html#_video_decode_commands
    numReferenceFrames = std::min(numReferenceFrames*2, videoCapabilities.maxActiveReferencePictures);
}
