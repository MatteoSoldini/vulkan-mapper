#include "../include/vk_video.h"
#include "../include/read_file.h"

#define MINIMP4_IMPLEMENTATION
#ifdef _WIN32
#include <sys/types.h>
#include <stddef.h>
typedef size_t ssize_t;
#endif
#include <minimp4.h>

#include <iostream>

#define H264_IMPLEMENTATION
#include "../include/h264.h"
#include <algorithm>

#include "../include/vk_engine.h"
#include "../include/vk_utils.h"

static int read_callback(int64_t offset, void* buffer, size_t size, void* token) {
    INPUT_BUFFER* buf = (INPUT_BUFFER*)token;
    size_t to_copy = MINIMP4_MIN(size, buf->size - offset - size);
    memcpy(buffer, buf->buffer + offset, to_copy);
    return to_copy != size;
}

void VulkanVideo::createVideoSession() {    
    // create video session
    VkVideoSessionCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
    info.queueFamilyIndex = sharedEngineState.videoFamily;
    info.maxActiveReferencePictures = numReferenceFrames;
    info.maxDpbSlots = numDpbSlots;
    info.maxCodedExtent.width = std::min(width, videoCapabilities.maxCodedExtent.width);
    info.maxCodedExtent.height = std::min(height, videoCapabilities.maxCodedExtent.height);
    info.pictureFormat = FRAME_FORMAT;
    info.referencePictureFormat = info.pictureFormat;
    info.pVideoProfile = &videoProfile;
    info.pStdHeaderVersion = &videoCapabilities.stdHeaderVersion;


    if (vkCreateVideoSessionKHR(sharedEngineState.device, &info, nullptr, &videoSession) != VK_SUCCESS) {
        throw std::runtime_error("failed to create video session");
    }

    // query memory requirements
    uint32_t requirementsCount = 0;
    vkGetVideoSessionMemoryRequirementsKHR(sharedEngineState.device, videoSession, &requirementsCount, nullptr);
    std::vector<VkVideoSessionMemoryRequirementsKHR> videoSessionRequirements(requirementsCount);

    for (auto& videoSessionRequirement : videoSessionRequirements) {
        videoSessionRequirement.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
    }

    if (vkGetVideoSessionMemoryRequirementsKHR(sharedEngineState.device, videoSession, &requirementsCount, videoSessionRequirements.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to get video session memory requirements");
    }

    // allocate and bind memory
    //internal_state->allocations.resize(requirement_count);
    for (uint32_t i = 0; i < requirementsCount; ++i) {
        VkMemoryRequirements memoryRequirements = videoSessionRequirements[i].memoryRequirements;
        
        // allocate memory
        VkDeviceMemory memory;
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;

        allocInfo.memoryTypeIndex = findGenericMemoryType(
            sharedEngineState.physicalDevice,
            memoryRequirements.memoryTypeBits
        );

        if (vkAllocateMemory(sharedEngineState.device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate memory");
        }

        // bind memory
        VkBindVideoSessionMemoryInfoKHR bindInfo = {};
        bindInfo.sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
        bindInfo.memory = memory;
        bindInfo.memoryBindIndex = videoSessionRequirements[i].memoryBindIndex;
        bindInfo.memoryOffset = 0;
        bindInfo.memorySize = allocInfo.allocationSize;

        if (vkBindVideoSessionMemoryKHR(sharedEngineState.device, videoSession, 1, &bindInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to bind video session memory");
        }
    }

    // create session parameters
    VkVideoDecodeH264SessionParametersAddInfoKHR sessionParametersAddInfoH264 = {};
    sessionParametersAddInfoH264.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR;
    sessionParametersAddInfoH264.stdPPSCount = ppsCount;
    sessionParametersAddInfoH264.pStdPPSs = ppsArrayH264.data();
    sessionParametersAddInfoH264.stdSPSCount = spsCount;
    sessionParametersAddInfoH264.pStdSPSs = spsArrayH264.data();
    
    VkVideoDecodeH264SessionParametersCreateInfoKHR sessionParametersInfoH264 = {};
    sessionParametersInfoH264.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR;
    sessionParametersInfoH264.maxStdPPSCount = ppsCount;
    sessionParametersInfoH264.maxStdSPSCount = spsCount;
    sessionParametersInfoH264.pParametersAddInfo = &sessionParametersAddInfoH264;

    VkVideoSessionParametersCreateInfoKHR sessionParametersInfo = {};
    sessionParametersInfo.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;
    sessionParametersInfo.videoSession = videoSession;
    sessionParametersInfo.videoSessionParametersTemplate = VK_NULL_HANDLE;
    sessionParametersInfo.pNext = &sessionParametersInfoH264;

    VkVideoSessionParametersKHR sessionParameters;
    if (vkCreateVideoSessionParametersKHR(sharedEngineState.device, &sessionParametersInfo, nullptr, &sessionParameters) != VK_SUCCESS) {
        throw std::runtime_error("failed to create video session parameters");
    }
}

void VulkanVideo::createTextures() {
    decodedPictureBuffer.resize(numDpbSlots);

    VkImage image;
    VkDeviceMemory imageMemory;

    VkVideoProfileListInfoKHR profileListInfo = {};
    profileListInfo.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
    profileListInfo.pProfiles = &videoProfile;
    profileListInfo.profileCount = 1;

    VkImageUsageFlags usageFlags = 0;
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    usageFlags |= VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
    usageFlags |= VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
    usageFlags |= VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;

    
    pDevice->createImage(
        width,
        height,
        FRAME_FORMAT,
        VK_IMAGE_TILING_OPTIMAL,
        usageFlags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        image,
        imageMemory,
        &profileListInfo
    );

    pDevice->transitionImageLayout(image, FRAME_FORMAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VulkanVideo::decodeFrame() {
    const FrameInfo currentFrameInfo = frameInfos[currentFrame];
    const h264::SliceHeader* frameSliceHeader = (const h264::SliceHeader*)frameSliceHeaderData.data() + currentFrame;
    const h264::PPS* pps = (const h264::PPS*)ppsData.data() + frameSliceHeader->pic_parameter_set_id;
    const h264::SPS* sps = (const h264::SPS*)spsData.data() + pps->seq_parameter_set_id;

    VkCommandBuffer commandBuffer = pDevice->getCommandBuffers()[0];    //CHECK

    VkVideoDecodeInfoKHR decodeInfo = {};
    decodeInfo.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR;
    decodeInfo.srcBuffer = nullptr; //stream_internal->resource;
    decodeInfo.srcBufferOffset = (VkDeviceSize)currentFrameInfo.offset;
    decodeInfo.srcBufferRange = 0;//(VkDeviceSize)AlignTo(op->stream_size, VIDEO_DECODE_BITSTREAM_ALIGNMENT);
    decodeInfo.dstPictureResource = 0;//*reference_slot_infos[op->current_dpb].pPictureResource;
    decodeInfo.referenceSlotCount = 0;//;
    decodeInfo.pReferenceSlots = nullptr;//decodeInfo.referenceSlotCount == 0 ? nullptr : reference_slots;
    decodeInfo.pSetupReferenceSlot = nullptr;//&reference_slot_infos[op->current_dpb];

    // run command
    vkCmdDecodeVideoKHR(commandBuffer, &decodeInfo);

    // advance frame
    currentFrame = ++currentFrame & framesCount;
}

VulkanVideo::VulkanVideo(VulkanEngine* pDevice, VideoSharedEngineState sharedEngineState, std::string filePath) {
    VulkanVideo::pDevice = pDevice;
    VulkanVideo::sharedEngineState = sharedEngineState;

    loadVideo(filePath);
    queryDecodeVideoCapabilities();
    loadVideoData();
    createVideoSession();
    createTextures();
}

void VulkanVideo::loadVideo(std::string filePath) {
    auto data = readFile(filePath);
	uint8_t* inputBuf = (uint8_t*)data.data();
    size_t inputSize = data.size();

    INPUT_BUFFER buf = { inputBuf, inputSize };
    MP4D_demux_t mp4 = { 0, };
    MP4D_open(&mp4, read_callback, &buf, inputSize);
    
    for (uint32_t ntrack = 0; ntrack < mp4.track_count; ntrack++) {
        MP4D_track_t& track = mp4.track[ntrack];
        std::vector<uint8_t> dump;

        // only video
        if (track.handler_type != MP4D_HANDLER_TYPE_VIDE) {
            std::cout << "skipping non video track" << std::endl;
            break;
        }

        // only H264
        if (track.object_type_indication != MP4_OBJECT_TYPE_AVC) {
            std::cout << "skipping non H264 codec" << std::endl;
            break;
        }

        // get video info
        width = track.SampleDescription.video.width;
        height = track.SampleDescription.video.height;
        bitRate = track.avg_bitrate_bps;

        // get sequence parameter set (sps)
        const void* data = nullptr;
        int index = 0;
        int size = 0;
        while (data = MP4D_read_sps(&mp4, ntrack, index, &size)) {
            const uint8_t* sps_data = (const uint8_t*)data;
            
            h264::Bitstream bs = {};
            bs.init(sps_data, size);
            h264::NALHeader nal = {};
            h264::read_nal_header(&nal, &bs);
            assert(nal.type = h264::NAL_UNIT_TYPE_SPS);

            h264::SPS sps = {};
            h264::read_sps(&sps, &bs);

            // Some validation checks that data parsing returned expected values:
            //	https://stackoverflow.com/questions/6394874/fetching-the-dimensions-of-a-h264video-stream
            uint32_t width = ((sps.pic_width_in_mbs_minus1 + 1) * 16) - sps.frame_crop_left_offset * 2 - sps.frame_crop_right_offset * 2;
            uint32_t height = ((2 - sps.frame_mbs_only_flag) * (sps.pic_height_in_map_units_minus1 + 1) * 16) - (sps.frame_crop_top_offset * 2) - (sps.frame_crop_bottom_offset * 2);
            assert(track.SampleDescription.video.width == width);
            assert(track.SampleDescription.video.height == height);
            //VulkanVideo::width = (sps.pic_width_in_mbs_minus1 + 1) * 16;
            //VulkanVideo::height = (sps.pic_height_in_map_units_minus1 + 1) * 16;
            numDpbSlots = std::max(numDpbSlots, uint32_t(sps.num_ref_frames + 1));

            spsData.resize(spsData.size() + sizeof(sps));
            std::memcpy((h264::SPS*)spsData.data() + spsCount, &sps, sizeof(sps));
            spsCount++;
            index++;
        }

        // get picture parameter set (pps)
        index = 0;
        size = 0;
        while (data = MP4D_read_pps(&mp4, ntrack, index, &size)) {
            const uint8_t* pps_data = (const uint8_t*)data;

            h264::Bitstream bs = {};
            bs.init(pps_data, size);
            h264::NALHeader nal = {};
            h264::read_nal_header(&nal, &bs);
            assert(nal.type = h264::NAL_UNIT_TYPE_PPS);

            h264::PPS pps = {};
            h264::read_pps(&pps, &bs);
            ppsData.resize(ppsData.size() + sizeof(pps));
            std::memcpy((h264::PPS*)ppsData.data() + ppsCount, &pps, sizeof(pps));
            ppsCount++;
            index++;
        }

        // get samples (frames)
        framesCount = track.sample_count;
        frameInfos.reserve(framesCount);
        frameSliceHeaderData.reserve(framesCount * sizeof(h264::SliceHeader));
        
        const h264::PPS* ppsArray = (const h264::PPS*)ppsData.data();
        const h264::SPS* spsArray = (const h264::SPS*)spsData.data();

        uint32_t trackDuration = 0;
        uint64_t alignedSize = 0;

        int prevPicOrderCntLSB = 0;
        int prevPicOrderCntMSB = 0;
        int pocCycle = 0;
        
        double timescaleRcp = 1.0 / double(track.timescale);

        for (uint32_t i = 0; i < framesCount; i++) {
            unsigned frameBytes, timestamp, duration;
            MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, ntrack, i, &frameBytes, &timestamp, &duration);
            trackDuration += duration;

            FrameInfo info = {};
            info.offset = alignedSize;

            const uint8_t* srcBuffer = inputBuf + ofs;
            while (frameBytes > 0) {
                uint32_t size = ((uint32_t)srcBuffer[0] << 24) | ((uint32_t)srcBuffer[1] << 16) | ((uint32_t)srcBuffer[2] << 8) | srcBuffer[3];
                size += 4;
                assert(frameBytes >= size);

                h264::Bitstream bs = {};
                bs.init(&srcBuffer[4], frameBytes);
                h264::NALHeader nal = {};
                h264::read_nal_header(&nal, &bs);

                if (nal.type == h264::NAL_UNIT_TYPE_CODED_SLICE_IDR) {
                    info.type = FrameType::IntraFrame;
                } else if (nal.type == h264::NAL_UNIT_TYPE_CODED_SLICE_NON_IDR) {
                    info.type = FrameType::PredictiveFrame;
                } else {
                    // Continue search for frame beginning NAL unit:
                    frameBytes -= size;
                    srcBuffer += size;
                    continue;
                }

                h264::SliceHeader* sliceHeader = (h264::SliceHeader*)frameSliceHeaderData.data() + i;
                *sliceHeader = {};
                h264::read_slice_header(sliceHeader, &nal, ppsArray, spsArray, &bs);

                const h264::PPS& pps = ppsArray[sliceHeader->pic_parameter_set_id];
                const h264::SPS& sps = spsArray[pps.seq_parameter_set_id];

                // Rec. ITU-T H.264 (08/2021) page 77
                int maxPicOrderCntLSB = 1 << (sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
                int picOrderCntLSB = sliceHeader->pic_order_cnt_lsb;

                if (picOrderCntLSB == 0) {
                    pocCycle++;
                }

                // Rec. ITU-T H.264 (08/2021) page 115
                // Also: https://www.ramugedia.com/negative-pocs
                int picOrderCntMSB = 0;
                if (picOrderCntLSB < prevPicOrderCntLSB && (prevPicOrderCntLSB - picOrderCntLSB) >= maxPicOrderCntLSB / 2) {
                    picOrderCntMSB = prevPicOrderCntMSB + maxPicOrderCntLSB; // pic_order_cnt_lsb wrapped around
                }
                else if (picOrderCntLSB > prevPicOrderCntLSB && (picOrderCntLSB - prevPicOrderCntLSB) > maxPicOrderCntLSB / 2) {
                    picOrderCntMSB = prevPicOrderCntMSB - maxPicOrderCntLSB; // here negative POC might occur
                }
                else {
                    picOrderCntMSB = prevPicOrderCntMSB;
                }
                //pic_order_cnt_msb = pic_order_cnt_msb % 256;
                prevPicOrderCntLSB = picOrderCntLSB;
                prevPicOrderCntMSB = picOrderCntMSB;

                // https://www.vcodex.com/h264avc-picture-management/
                info.poc = picOrderCntMSB + picOrderCntLSB; // poc = TopFieldOrderCount
                info.gop = pocCycle - 1;

                // Accept frame beginning NAL unit:
                info.referencePriority = nal.idc;
                info.size = sizeof(h264::nal_start_code) + size - 4;
                break;
            }

            info.timestampSeconds = float(double(timestamp) * timescaleRcp);
            info.durationSeconds = float(double(duration) * timescaleRcp);

            // push to array
            frameInfos.push_back(info);
        }

        // display order
        frameDisplayOrder.resize(frameInfos.size());
        for (size_t i = 0; i < frameInfos.size(); i++) {
            frameDisplayOrder[i] = i;
        }
        std::sort(frameDisplayOrder.begin(), frameDisplayOrder.end(), [&](size_t a, size_t b) {
            const FrameInfo& frameA = frameInfos[a];
            const FrameInfo& frameB = frameInfos[b];
            int64_t prioA = (int64_t(frameA.gop) << 32ll) | int64_t(frameA.poc);
            int64_t prioB = (int64_t(frameB.gop) << 32ll) | int64_t(frameB.poc);
            return prioA < prioB;
            });
        for (size_t i = 0; i < frameDisplayOrder.size(); ++i) {
            frameInfos[frameDisplayOrder[i]].displayOrder = (int)i;
        }

        averageFrameRate = float(double(track.timescale) / double(trackDuration) * track.sample_count);
        durationSeconds = float(double(trackDuration) * timescaleRcp);


        // copy video track
        VkDeviceSize bufferSize = alignedSize;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        pDevice->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        VkDevice device = pDevice->getDevice();

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);

        for (uint32_t i = 0; i < track.sample_count; i++) {
            unsigned frameBytes, timestamp, duration;
            MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, ntrack, i, &frameBytes, &timestamp, &duration);
            uint8_t* dstBuffer = (uint8_t*)data + frameInfos[i].offset;
            const uint8_t* srcBuffer = inputBuf + ofs;
            while (frameBytes > 0) {
                uint32_t size = ((uint32_t)srcBuffer[0] << 24) | ((uint32_t)srcBuffer[1] << 16) | ((uint32_t)srcBuffer[2] << 8) | srcBuffer[3];
                size += 4;
                assert(frameBytes >= size);

                h264::Bitstream bs = {};
                bs.init(&srcBuffer[4], sizeof(uint8_t));
                h264::NALHeader nal = {};
                h264::read_nal_header(&nal, &bs);

                if (
                    nal.type != h264::NAL_UNIT_TYPE_CODED_SLICE_IDR &&
                    nal.type != h264::NAL_UNIT_TYPE_CODED_SLICE_NON_IDR
                ) {
                    frameBytes -= size;
                    srcBuffer += size;
                    continue;
                }

                std::memcpy(dstBuffer, h264::nal_start_code, sizeof(h264::nal_start_code));
                std::memcpy(dstBuffer + sizeof(h264::nal_start_code), srcBuffer + 4, size - 4);
                break;
            }
        }
        
        vkUnmapMemory(device, stagingBufferMemory);

        pDevice->createBuffer(
            alignedSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            videoStreamBuffer,
            videoStreamBufferMemory
        );

        //copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    MP4D_close(&mp4);

    // upload to gpu
    // TODO
}

void VulkanVideo::queryDecodeVideoCapabilities() {
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

    if (vkGetPhysicalDeviceVideoCapabilitiesKHR(sharedEngineState.physicalDevice, &videoProfile, &videoCapabilities) != VK_SUCCESS) {
        throw std::runtime_error("failed to get device video capabilities");
    }
}

void VulkanVideo::loadVideoData() {
    // load pps array
    ppsArrayH264.resize(ppsCount);
    std::vector<StdVideoH264ScalingLists> scalinglistArrayH264(ppsCount);

    for (int i = 0; i < ppsCount; i++) {
        const h264::PPS* pps = (const h264::PPS*)ppsData.data() + i;
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
    spsArrayH264.resize(spsCount);
    std::vector<StdVideoH264SequenceParameterSetVui> vuiArrayH264(spsCount);
    std::vector<StdVideoH264HrdParameters> hrdArrayH264(spsCount);
    
    for (uint32_t i = 0; i < spsCount; ++i) {
        const h264::SPS* sps = (const h264::SPS*)spsData.data() + i;
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
