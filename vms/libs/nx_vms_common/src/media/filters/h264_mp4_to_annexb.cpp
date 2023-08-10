// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "h264_mp4_to_annexb.h"

#include <array>

#include <nx/codec/nal_extradata.h>
#include <nx/codec/nal_units.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/log/log.h>

QnConstAbstractDataPacketPtr H2645Mp4ToAnnexB::processData(const QnConstAbstractDataPacketPtr& data)
{
    const QnConstCompressedVideoDataPtr videoData =
        std::dynamic_pointer_cast<const QnCompressedVideoData>(data);

    if (!videoData)
        return data;

    auto result = processVideoData(videoData);
    if (!result)
        return data;

    return result;
}

QnCompressedVideoDataPtr H2645Mp4ToAnnexB::processVideoData(
    const QnConstCompressedVideoDataPtr& videoData)
{
    auto codecId = videoData->compressionType;
    if (codecId != AV_CODEC_ID_H264 && codecId != AV_CODEC_ID_H265)
        return nullptr;

    if (!videoData->context)
    {
        NX_DEBUG(this, "Invalid video stream, failed to convert to AnnexB format, no extra data");
        return nullptr;
    }

    std::vector<uint8_t> header;
    const uint8_t* extraData = videoData->context->getExtradata();
    int extraDataSize = videoData->context->getExtradataSize();

    if (extraData && extraDataSize)
    {
        if (nx::media::nal::isStartCode(extraData, extraDataSize))
            return nullptr;
    }
    else
    {
        // Some sizes of NAL units can start from 001, so we try to check extradata first.
        if (nx::media::nal::isStartCode(videoData->data(), videoData->dataSize()))
            return nullptr;
    }

    if (!m_newContext || videoData->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
    {
        // Reading sequence header from extradata.
        if (codecId == AV_CODEC_ID_H264)
            header = nx::media::nal::readH264SeqHeaderFromExtraData(extraData, extraDataSize);
        else if (codecId == AV_CODEC_ID_H265)
            header = nx::media::nal::readH265SeqHeaderFromExtraData(extraData, extraDataSize);
        if (header.empty())
        {
            NX_DEBUG(this,
                "Invalid video stream, failed to convert to AnnexB format, invalid extra data");
            return nullptr;
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
