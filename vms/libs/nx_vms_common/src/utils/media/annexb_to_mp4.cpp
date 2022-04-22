// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "annexb_to_mp4.h"

#include <nx/utils/log/log.h>
#include <utils/media/h264_utils.h>
#include <utils/media/hevc/extradata.h>
#include <utils/media/nalUnits.h>
#include <utils/media/utils.h>


namespace nx::media {

QnCompressedVideoDataPtr convertStartCodesToSizes(const QnCompressedVideoData* videoData)
{
    const uint8_t* data = (const uint8_t*)videoData->data();
    int32_t size = videoData->dataSize();
    constexpr int kNaluSizeLenght = 4;
    auto nalUnits = nx::media::nal::findNalUnitsAnnexB(data, size);
    if (nalUnits.empty())
        return nullptr;

    int32_t resultSize = 0;
    for (const auto& nalu: nalUnits)
        resultSize += nalu.size + kNaluSizeLenght;

    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData());
    result->QnCompressedVideoData::assign(videoData);

    result->m_data.resize(resultSize);
    char* resultData = result->m_data.data();
    for (const auto& nalu: nalUnits)
    {
        uint32_t* ptr = (uint32_t*)resultData;
        *ptr = htonl(nalu.size);
        memcpy(resultData + kNaluSizeLenght, nalu.data, nalu.size);
        resultData += nalu.size + kNaluSizeLenght;
    }
    return result;
}

bool isMp4Format(const QnCompressedVideoData* data)
{
    if (data->compressionType != AV_CODEC_ID_H264 && data->compressionType != AV_CODEC_ID_H265)
        return true;

    if (!data->context)
        return false;

    const uint8_t* extraData = data->context->getExtradata();
    int extraDataSize = data->context->getExtradataSize();

    if (extraData && extraDataSize && !nx::media::nal::isStartCode(extraData, extraDataSize))
        return true;

    return false;
}

QnCompressedVideoDataPtr AnnexbToMp4::process(const QnCompressedVideoData* frame)
{
    if (frame->compressionType != AV_CODEC_ID_H264 && frame->compressionType != AV_CODEC_ID_H265)
        return nullptr;

    if (isMp4Format(frame))
    {
        NX_WARNING(this, "Failed to convert from AnnexB to MP4 format, format is MP4 already");
        return nullptr;
    }

    if (frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
    {
        std::vector<uint8_t> extradata;
        if (frame->compressionType == AV_CODEC_ID_H264)
        {
            extradata = nx::media::h264::buildExtraDataMp4(
                (const uint8_t*)frame->data(), frame->dataSize());
        }
        else if (frame->compressionType == AV_CODEC_ID_H265)
        {
            extradata = nx::media::hevc::buildExtraDataMp4(
                (const uint8_t*)frame->data(), frame->dataSize());
        }
        else
            return nullptr;

        if (!m_context && extradata.empty())
        {
            NX_WARNING(this, "Failed to build extra data");
            return nullptr;
        }

        auto context = std::make_shared<CodecParameters>(frame->context->getAvCodecParameters());
        context->setExtradata(extradata.data(), extradata.size());

        QSize streamResolution = nx::media::getFrameSize(frame);
        if (streamResolution.isEmpty())
            return nullptr;

        context->getAvCodecParameters()->width = streamResolution.width();
        context->getAvCodecParameters()->height = streamResolution.height();

        m_context = context;
    }

    auto result = convertStartCodesToSizes(frame);
    if (!result)
    {
        NX_WARNING(this, "Failed to convert from AnnexB to MP4 format, invalid NAL units");
        return nullptr;
    }
    result->context = m_context;
    return result;
}

} // namespace nx::media