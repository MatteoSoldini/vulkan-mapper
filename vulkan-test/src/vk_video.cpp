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
#include "../include/vk_utils.h"

static int read_callback(int64_t offset, void* buffer, size_t size, void* token) {
    INPUT_BUFFER* buf = (INPUT_BUFFER*)token;
    size_t to_copy = MINIMP4_MIN(size, buf->size - offset - size);
    memcpy(buffer, buf->buffer + offset, to_copy);
    return to_copy != size;
}

void VulkanVideo::createVideoSession() {
    // query decode capabilities
    VkVideoDecodeH264ProfileInfoKHR decodeProfile = {};
    decodeProfile.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR;
    decodeProfile.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_HIGH;
    decodeProfile.pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR;

    VkVideoProfileInfoKHR videoProfile = {};
    videoProfile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
    videoProfile.pNext = &decodeProfile;
    videoProfile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
    videoProfile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    videoProfile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    videoProfile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;

    VkVideoDecodeH264CapabilitiesKHR h264Capabilities = {};
    h264Capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR;

    VkVideoDecodeCapabilitiesKHR decodeCapabilities = {};
    decodeCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
    decodeCapabilities.pNext = &h264Capabilities;

    VkVideoCapabilitiesKHR videoCapabilities = {};
    videoCapabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
    videoCapabilities.pNext = &decodeCapabilities;

    if (vkGetPhysicalDeviceVideoCapabilitiesKHR(sharedEngineState.physicalDevice, &videoProfile, &videoCapabilities) != VK_SUCCESS) {
        throw std::runtime_error("failed to get device video capabilities");
    }

    // create video session
    uint32_t numReferenceFrames = videoCapabilities.maxActiveReferencePictures;    //TODO: should check actual max reference frame used by video

    VkVideoSessionCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
    info.queueFamilyIndex = sharedEngineState.videoFamily;
    info.maxActiveReferencePictures = numReferenceFrames; //*2: top and bottom field counts as two I think: https://vulkan.lunarg.com/doc/view/1.3.239.0/windows/1.3-extensions/vkspec.html#_video_decode_commands
    info.maxDpbSlots = std::min(numDpbSlots, videoCapabilities.maxDpbSlots);
    info.maxCodedExtent.width = std::min(width, videoCapabilities.maxCodedExtent.width);
    info.maxCodedExtent.height = std::min(height, videoCapabilities.maxCodedExtent.height);
    info.pictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;    //YUV420
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
    sessionParametersAddInfoH264.pStdPPSs = nullptr; //pps.data();
    sessionParametersAddInfoH264.stdSPSCount = spsCount;
    sessionParametersAddInfoH264.pStdSPSs = nullptr; //sps_array_h264.data();
    
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

VulkanVideo::VulkanVideo(VideoSharedEngineState sharedEngineState, std::string filePath) {
    VulkanVideo::sharedEngineState = sharedEngineState;
    
    loadVideo(filePath);
    createVideoSession();
    // video session
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

    }


    MP4D_close(&mp4);

    // upload to gpu
    // TODO
}
