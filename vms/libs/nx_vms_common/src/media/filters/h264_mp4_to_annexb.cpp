// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "h264_mp4_to_annexb.h"

#include <array>

#include <utils/media/nalUnits.h>
#include <utils/media/hevc/hevc_decoder_configuration_record.h>
#include <nx/utils/log/log.h>
#include <nx/network/socket_common.h>
#include <nx/streaming/video_data_packet.h>

namespace {

int getNalSize(const uint8_t* ptr)
{
    return (ptr[0] << 8) | ptr[1];
}

void appendNalUnit(std::vector<uint8_t>& result, const uint8_t* data, int size)
{
    result.insert(result.end(), nx::media::nal::kStartCode.begin(), nx::media::nal::kStartCode.end());
    result.insert(result.end(), data, data + size);
}

// TODO: Code duplication with "h264_utils.cpp".
std::vector<uint8_t> readH264SeqHeaderFromExtraData(const uint8_t* extraData, int extraDataSize)
{
    std::vector<uint8_t> result;
    if (!extraData || extraDataSize < 8 || extraData[0] != 1)
        return result;

    const uint8_t* p = extraData;
    //sps & pps is in the extradata, parsing it...
    //following code has been taken from libavcodec/h264.c

    int lengthSizeMinusOne = (extraData[4] & 0x03);
    if (lengthSizeMinusOne != 3)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unsupported NAL lenght size: %1, TODO impl this",
            lengthSizeMinusOne + 1);
        return result;
    }

    // Decode sps from avcC
    int cnt = *(p + 5) & 0x1f; // Number of sps
    p += 6;

    for (int i = 0; i < cnt; i++)
    {
        const int nalsize = getNalSize(p);
        p += 2; //skipping nalusize
        if (nalsize > extraDataSize - (p - extraData))
            break;
        appendNalUnit(result, p, nalsize);
        p += nalsize;
    }

    // Decode pps from avcC
    cnt = *(p++); // Number of pps
    for (int i = 0; i < cnt; ++i)
    {
        const int nalsize = getNalSize(p);
        p += 2;
        if (nalsize > extraDataSize - (p - extraData))
            break;

        appendNalUnit(result, p, nalsize);
        p += nalsize;
    }
    return result;
}

} // namespace

std::vector<uint8_t> readH265SeqHeaderFromExtraData(const uint8_t* extraData, int extraDataSize)
{
    std::vector<uint8_t> result;
    if (!extraData)
        return result;

    nx::media::hevc::HEVCDecoderConfigurationRecord hvcc;
    if (!hvcc.read(extraData, extraDataSize))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to parse hvcc");
        return result;
    }
    if (hvcc.lengthSizeMinusOne != 3)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unsupported NAL lenght size: %1, TODO impl this",
            hvcc.lengthSizeMinusOne + 1);
        return result;
    }
    for (auto& vps: hvcc.vps)
        appendNalUnit(result, vps.data(), vps.size());
    for (auto& sps: hvcc.sps)
        appendNalUnit(result, sps.data(), sps.size());
    for (auto& pps: hvcc.pps)
        appendNalUnit(result, pps.data(), pps.size());
    return result;
}

QnConstAbstractDataPacketPtr H2645Mp4ToAnnexB::processData(const QnConstAbstractDataPacketPtr& data)
{
    const QnConstCompressedVideoDataPtr videoData =
        std::dynamic_pointer_cast<const QnCompressedVideoData>(data);

    if (!videoData)
        return data;
    return processVideoData(videoData);
}

QnConstCompressedVideoDataPtr H2645Mp4ToAnnexB::processVideoData(
    const QnConstCompressedVideoDataPtr& videoData)
{
    auto codecId = videoData->compressionType;
    if (codecId != AV_CODEC_ID_H264 && codecId != AV_CODEC_ID_H265)
        return videoData;

    if (!videoData->context)
    {
        NX_DEBUG(this, "Invalid video stream, failed to convert to AnnexB format, no extra data");
        return videoData;
    }

    std::vector<uint8_t> header;
    const uint8_t* extraData = videoData->context->getExtradata();
    int extraDataSize = videoData->context->getExtradataSize();

    if (extraData && extraDataSize)
    {
        if (nx::media::nal::isStartCode(extraData, extraDataSize))
            return videoData;
    }
    else
    {
        // Some sizes of NAL units can start from 001, so we try to check extradata first.
        if (nx::media::nal::isStartCode(videoData->data(), videoData->dataSize()))
            return videoData;
    }

    if (videoData->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
    {
        // Reading sequence header from extradata.
        if (codecId == AV_CODEC_ID_H264)
            header = readH264SeqHeaderFromExtraData(extraData, extraDataSize);
        else if (codecId == AV_CODEC_ID_H265)
            header = readH265SeqHeaderFromExtraData(extraData, extraDataSize);
        if (header.empty())
        {
            NX_DEBUG(this,
                "Invalid video stream, failed to convert to AnnexB format, invalid extra data");
            return videoData;
        }

        auto context = std::make_shared<CodecParameters>(
            videoData->context->getAvCodecParameters());
        context->setExtradata(header.data(), header.size());

        m_newContext = context;
    }

    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData());
    result->QnCompressedVideoData::assign(videoData.get());

    const int resultDataSize = header.size() + videoData->dataSize();
    result->m_data.resize(resultDataSize);
    uint8_t* const resultData = (uint8_t*)result->m_data.data();
    memcpy(resultData, header.data(), header.size());
    memcpy(resultData + header.size(), videoData->data(), videoData->dataSize());

    // Replacing NALU size with {0 0 0 1}.
    nx::media::nal::convertToStartCodes(resultData + header.size(), videoData->dataSize());
    result->context = m_newContext;
    return result;
}
