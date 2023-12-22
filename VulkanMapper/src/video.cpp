#include "../include/video.h"
#include "../include/read_file.h"

#include <h264.h>
#include <minimp4.h>

#include <iostream>
#include <algorithm>

static int read_callback(int64_t offset, void* buffer, size_t size, void* token) {
    INPUT_BUFFER* buf = (INPUT_BUFFER*)token;
    size_t to_copy = MINIMP4_MIN(size, buf->size - offset - size);
    memcpy(buffer, buf->buffer + offset, to_copy);
    return to_copy != size;
}

Video::Video(MediaId_t id, VulkanState* pDevice, std::string filePath) : Media(id, filePath) {
    Video::pDevice = pDevice;
    pVkDecoder = new VulkanVideo(pDevice);

    vmVideoFrameStreamId = pDevice->createVideoFrameStream();

    // query video capabilities
    uint64_t bitStreamAlignment = pVkDecoder->queryDecodeVideoCapabilities();

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
        frameInfos.resize(framesCount);
        frameSliceHeaderData.reserve(framesCount * sizeof(h264::SliceHeader));

        const h264::PPS* ppsArray = (const h264::PPS*)ppsData.data();
        const h264::SPS* spsArray = (const h264::SPS*)spsData.data();

        uint32_t trackDuration = 0;

        // aligned bitstream size
        bitStreamSize = 0;

        int prevPicOrderCntLSB = 0;
        int prevPicOrderCntMSB = 0;
        int pocCycle = 0;

        double timescaleRcp = 1.0 / double(track.timescale);

        for (uint32_t i = 0; i < framesCount; i++) {
            unsigned frameBytes, timestamp, duration;
            MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, ntrack, i, &frameBytes, &timestamp, &duration);
            trackDuration += duration;

            FrameInfo& info = frameInfos[i];
            info.offset = bitStreamSize;

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

            bitStreamSize += ((info.size + bitStreamAlignment - 1) / bitStreamAlignment) * bitStreamAlignment;
            info.timestampSeconds = float(double(timestamp) * timescaleRcp);
            info.durationSeconds = float(double(duration) * timescaleRcp);

            std::cout << info.timestampSeconds << "\t" << info.type << "\t" << info.size << std::endl;
        }

        // display order
        std::vector<size_t> frameDisplayOrder;
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

        // assemble stream data
        std::vector<uint8_t> streamData;
        streamData.resize(bitStreamSize);
        for (uint32_t i = 0; i < framesCount; i++) {
            unsigned frameBytes, timestamp, duration;
            MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, ntrack, i, &frameBytes, &timestamp, &duration);
            uint8_t* dstBuffer = (uint8_t*)streamData.data() + frameInfos[i].offset;
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

        pVkDecoder->loadVideoStream(streamData.data(), streamData.size());
        pVkDecoder->setupDecoder(this);
    }

    MP4D_close(&mp4);

    // decode first frame
    decodeFrame();
}

void Video::decodeFrame() {
    if (decodingResult != nullptr) {     // if waiting for a frame to be decoded
        std::chrono::steady_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
        float timePassed = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        if (
            vkGetFenceStatus(pDevice->getDevice(), decodingResult->decodeFence) == VK_SUCCESS &&    // finished decoding
            (
                (timePassed >= frameInfos[currentFrame].timestampSeconds && playing)                // time to emit (or)
                || presentAFrame                                                                    // present a frame anyway
                )
            ) {
            vkResetFences(pDevice->getDevice(), 1, &(decodingResult->decodeFence));
            pDevice->loadVideoFrame(vmVideoFrameStreamId, decodingResult->frameImageView);  // emit frame

            delete decodingResult;
            decodingResult = nullptr;

            if (presentAFrame) presentAFrame = false;
        }
        else {
            //std::cout << "time remaining for frame " << currentFrame << ": " << frameInfos[currentFrame].timestampSeconds - timePassed << std::endl;
            return;    // frame still decoding, skip
        }
    }
    else {  // decode next frame
        FrameInfo currentFrameInfo = frameInfos[currentFrame];

        // reset dpb on intra frame
        if (currentFrameInfo.type == FrameType::IntraFrame) {
            //std::cout << "intra frame" << std::endl;
            referencesPositions.clear();
            currentDecodePosition = 0;
            nextDecodePosition = 0;
        }

        // update dpb position
        currentDecodePosition = nextDecodePosition;

        // vk decode
        decodingResult = pVkDecoder->decodeFrame(this);

        // dpb management
        if (currentFrameInfo.referencePriority > 0) {   // if frame is used as reference
            if (referencesPositions.size() < 1) {
                referencesPositions.resize(1);
                //referenceSlotPositions.resize(referenceSlotPositions.size() + 1);
            }
            referencesPositions[0] = currentDecodePosition;
            nextDecodePosition = ++(nextDecodePosition) % numDpbSlots;
            // ignoring wickedengine's next_slot
        }

        // advance frame
        currentFrame = ++(currentFrame) % framesCount;
        if (currentFrame == 0) startTime = std::chrono::high_resolution_clock::now();
    }
}

void Video::pause() {
    playing = false;
}

void Video::play() {    
    playing = true;
    std::chrono::steady_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float>(frameInfos[currentFrame].timestampSeconds);
    auto castedDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    startTime = currentTime - castedDuration;
    //float timePassed = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
}

void Video::firstFrame() {
    currentFrame = 0;

    std::chrono::steady_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
    startTime = currentTime;

    presentAFrame = true;
}
