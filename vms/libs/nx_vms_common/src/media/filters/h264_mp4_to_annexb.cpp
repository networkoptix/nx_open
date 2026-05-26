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
    const QnConstCompressedVideoDataPtr& videoData,
    bool forceSharedMemory,
    const nx::media::ffmpeg::FfmpegSharedMemoryAllocatorPtr& sharedMemoryAllocator)
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

    const int resultDataSize = header.size() + videoData->dataSize();
    QnCompressedVideoDataPtr resultBase;
    if (forceSharedMemory)
    {
        resultBase = createSharedMemoryCompressedVideoData(
            resultDataSize,
            sharedMemoryAllocator);
        if (!resultBase)
        {
            NX_WARNING(
                this,
                "Failed to allocate %1 bytes in shared memory for AnnexB conversion",
                resultDataSize);
            return nullptr;
        }
    }
    else
    {
        resultBase = QnCompressedVideoDataPtr(new QnWritableCompressedVideoData());
    }

    resultBase->QnCompressedVideoData::assign(videoData.get());
    uint8_t* resultData = nullptr;
    if (forceSharedMemory)
    {
        auto shmResult = std::dynamic_pointer_cast<QnSharedMemoryCompressedVideoData>(resultBase);
        if (!shmResult || !shmResult->mutableData())
        {
            NX_WARNING(this, "Invalid shared-memory destination frame for AnnexB conversion");
            return nullptr;
        }
        resultData = reinterpret_cast<uint8_t*>(shmResult->mutableData());
    }
    else
    {
        auto writableResult = std::dynamic_pointer_cast<QnWritableCompressedVideoData>(resultBase);
        NX_ASSERT(writableResult);
        writableResult->m_data.resize(resultDataSize);
        resultData = reinterpret_cast<uint8_t*>(writableResult->m_data.data());
    }

    memcpy(resultData, header.data(), header.size());
    memcpy(resultData + header.size(), videoData->data(), videoData->dataSize());

    // Replacing NALU size with {0 0 0 1}.
    nx::media::nal::convertToStartCodes(resultData + header.size(), videoData->dataSize());
    resultBase->context = m_newContext;
    return resultBase;
}
