#include "../include/media_manager.h"
//#include "../include/vk_video.h"


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>
#include "../include/read_file.h"
#include <minimp4.h>
#include <iostream>
#include "../include/h264.h"
#include <algorithm>


void MediaManager::startNextFrameDecode() {
    //std::cout << "decoding frames at: " << 1.0f / timePassed << std::endl;
    //pVideoState->lastDecodeTime = std::chrono::high_resolution_clock::now();
}

MediaManager::MediaManager(VulkanEngine* pEngine) {
    MediaManager::pEngine = pEngine;
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
    newImage.type = MediaType::Image;
    newImage.filePath = filePath;

    medias.push_back(newImage);
}

void MediaManager::decodeFrames() {
    // check for any video that need decoding
    for (auto media : medias) {
        if (media.type == MediaType::Video) {   // VIDEO
            VideoState* pVideoState = dynamic_cast<VideoState*>(media.pState);
            if (pVideoState == nullptr) throw std::runtime_error("wrong media state");

            if (pVideoState->decodingResult != nullptr) {     // if waiting for a frame to be decoded
                std::chrono::steady_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
                float timePassed = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - pVideoState->startTime).count();
                
                if (
                    vkGetFenceStatus(pEngine->getDevice(), pVideoState->decodingResult->decodeFence) == VK_SUCCESS &&
                    timePassed >= pVideoState->frameInfos[pVideoState->currentFrame].timestampSeconds
                    ) {  // if the frame has been decoded and its time to emit
                    vkResetFences(pEngine->getDevice(), 1, &(pVideoState->decodingResult->decodeFence));
                    pEngine->loadVideoFrame(pVideoState->decodingResult->frameImageView);  // emit frame

                    delete pVideoState->decodingResult;
                    pVideoState->decodingResult = nullptr;
                }
                else break;    // frame still decoding, skip
            }
            else {  // decode next frame
                FrameInfo currentFrameInfo = pVideoState->frameInfos[pVideoState->currentFrame];

                // reset dpb on intra frame
                if (currentFrameInfo.type == FrameType::IntraFrame) {
                    //std::cout << "intra frame" << std::endl;
                    pVideoState->referencesPositions.clear();
                    pVideoState->currentDecodePosition = 0;
                    pVideoState->nextDecodePosition = 0;
                }

                // update dpb position
                pVideoState->currentDecodePosition = pVideoState->nextDecodePosition;

                // vk decode
                pVideoState->decodingResult = pVideoState->pVkDecoder->decodeFrame(pVideoState);

                // dpb management
                if (currentFrameInfo.referencePriority > 0) {   // if frame is used as reference
                    if (pVideoState->referencesPositions.size() < 1) {
                        pVideoState->referencesPositions.resize(1);
                        //referenceSlotPositions.resize(referenceSlotPositions.size() + 1);
                    }
                    pVideoState->referencesPositions[0] = pVideoState->currentDecodePosition;
                    pVideoState->nextDecodePosition = ++(pVideoState->nextDecodePosition) % pVideoState->numDpbSlots;
                    // ignoring wickedengine's next_slot
                }

                // advance frame
                pVideoState->currentFrame = ++(pVideoState->currentFrame) % pVideoState->framesCount;
                if (pVideoState->currentFrame == 0) pVideoState->startTime = std::chrono::high_resolution_clock::now();
            }
        }
    }
}

std::vector<Media> MediaManager::getMedias() {
    return medias;
}

Media* MediaManager::getMediaById(m_id mediaId) {
    for (Media media : medias) {
        if (media.id == mediaId) {
            return &media;
        }
    }

    return nullptr;
}

static int read_callback(int64_t offset, void* buffer, size_t size, void* token) {
    INPUT_BUFFER* buf = (INPUT_BUFFER*)token;
    size_t to_copy = MINIMP4_MIN(size, buf->size - offset - size);
    memcpy(buffer, buf->buffer + offset, to_copy);
    return to_copy != size;
}

void MediaManager::loadVideo(std::string filePath) {
    // add video to media
    m_id id = newId();

    Media media{};
    media.id = id;
    media.type = MediaType::Video;
    media.filePath = filePath;
    media.pState = new VideoState;
    medias.push_back(media);

    VideoState& videoState = *(media.pState);
    videoState.pVkDecoder = new VulkanVideo(pEngine);
    
    // query video capabilities
    uint64_t bitStreamAlignment = videoState.pVkDecoder->queryDecodeVideoCapabilities();

    // load data
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
        videoState.width = track.SampleDescription.video.width;
        videoState.height = track.SampleDescription.video.height;
        videoState.bitRate = track.avg_bitrate_bps;

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
            videoState.numDpbSlots = std::max(videoState.numDpbSlots, uint32_t(sps.num_ref_frames + 1));

            videoState.spsData.resize(videoState.spsData.size() + sizeof(sps));
            std::memcpy((h264::SPS*)videoState.spsData.data() + videoState.spsCount, &sps, sizeof(sps));
            videoState.spsCount++;
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
            videoState.ppsData.resize(videoState.ppsData.size() + sizeof(pps));
            std::memcpy((h264::PPS*)videoState.ppsData.data() + videoState.ppsCount, &pps, sizeof(pps));
            videoState.ppsCount++;
            index++;
        }

        // get samples (frames)
        videoState.framesCount = track.sample_count;
        videoState.frameInfos.resize(videoState.framesCount);
        videoState.frameSliceHeaderData.reserve(videoState.framesCount * sizeof(h264::SliceHeader));

        const h264::PPS* ppsArray = (const h264::PPS*)videoState.ppsData.data();
        const h264::SPS* spsArray = (const h264::SPS*)videoState.spsData.data();

        uint32_t trackDuration = 0;

        // aligned bitstream size
        videoState.bitStreamSize = 0;

        int prevPicOrderCntLSB = 0;
        int prevPicOrderCntMSB = 0;
        int pocCycle = 0;

        double timescaleRcp = 1.0 / double(track.timescale);

        for (uint32_t i = 0; i < videoState.framesCount; i++) {
            unsigned frameBytes, timestamp, duration;
            MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, ntrack, i, &frameBytes, &timestamp, &duration);
            trackDuration += duration;

            FrameInfo& info = videoState.frameInfos[i];
            info.offset = videoState.bitStreamSize;

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
                }
                else if (nal.type == h264::NAL_UNIT_TYPE_CODED_SLICE_NON_IDR) {
                    info.type = FrameType::PredictiveFrame;
                }
                else {
                    // Continue search for frame beginning NAL unit:
                    frameBytes -= size;
                    srcBuffer += size;
                    continue;
                }

                h264::SliceHeader* sliceHeader = (h264::SliceHeader*)videoState.frameSliceHeaderData.data() + i;
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

            videoState.bitStreamSize += ((info.size + bitStreamAlignment - 1) / bitStreamAlignment) * bitStreamAlignment;
            info.timestampSeconds = float(double(timestamp) * timescaleRcp);
            info.durationSeconds = float(double(duration) * timescaleRcp);

            std::cout << info.timestampSeconds << "\t" << info.type << "\t" << info.size << std::endl;
        }

        // display order
        std::vector<size_t> frameDisplayOrder;
        frameDisplayOrder.resize(videoState.frameInfos.size());
        for (size_t i = 0; i < videoState.frameInfos.size(); i++) {
            frameDisplayOrder[i] = i;
        }
        std::sort(frameDisplayOrder.begin(), frameDisplayOrder.end(), [&](size_t a, size_t b) {
            const FrameInfo& frameA = videoState.frameInfos[a];
            const FrameInfo& frameB = videoState.frameInfos[b];
            int64_t prioA = (int64_t(frameA.gop) << 32ll) | int64_t(frameA.poc);
            int64_t prioB = (int64_t(frameB.gop) << 32ll) | int64_t(frameB.poc);
            return prioA < prioB;
            });
        for (size_t i = 0; i < frameDisplayOrder.size(); ++i) {
            videoState.frameInfos[frameDisplayOrder[i]].displayOrder = (int)i;
        }

        videoState.averageFrameRate = float(double(track.timescale) / double(trackDuration) * track.sample_count);
        videoState.durationSeconds = float(double(trackDuration) * timescaleRcp);

        // assemble stream data
        std::vector<uint8_t> streamData;
        streamData.resize(videoState.bitStreamSize);
        for (uint32_t i = 0; i < videoState.framesCount; i++) {
            unsigned frameBytes, timestamp, duration;
            MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, ntrack, i, &frameBytes, &timestamp, &duration);
            uint8_t* dstBuffer = (uint8_t*)streamData.data() + videoState.frameInfos[i].offset;
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

        videoState.pVkDecoder->loadVideoStream(streamData.data(), streamData.size());
        videoState.pVkDecoder->setupDecoder(&videoState);
    }

    MP4D_close(&mp4);
}