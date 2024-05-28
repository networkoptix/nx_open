// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "annexb_to_mp4.h"

#include <nx/codec/hevc/extradata.h>
#include <nx/codec/nal_units.h>
#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/utils/log/log.h>

namespace nx::media {

std::vector<uint8_t> buildExtraDataMp4(const QnCompressedVideoData* frame)
{
    std::vector<uint8_t> extradata;
    auto extraDataBuilder = frame->compressionType == AV_CODEC_ID_H264
        ? nx::media::h264::buildExtraDataMp4FromAnnexB
        : nx::media::hevc::buildExtraDataMp4FromAnnexB;
    if (frame->context && frame->context->getExtradata() && frame->context->getExtradataSize())
    {
        extradata = extraDataBuilder(
            (const uint8_t*)frame->context->getExtradata(), frame->context->getExtradataSize());
    }
    if (extradata.empty())
    {
        extradata = extraDataBuilder((const uint8_t*)frame->data(), frame->dataSize());
    }
    return extradata;
}

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
        return false;

    if (!data->context)
        return false;

    const uint8_t* extraData = data->context->getExtradata();
    int extraDataSize = data->context->getExtradataSize();

    if (extraData && extraDataSize && !nx::media::nal::isStartCode(extraData, extraDataSize))
        return true;

    return false;
}

bool isAnnexb(const QnCompressedVideoData* data)
{
    if (data->compressionType != AV_CODEC_ID_H264 && data->compressionType != AV_CODEC_ID_H265)
        return false;

    return !isMp4Format(data);
}

void AnnexbToMp4::updateCodecParameters(const QnCompressedVideoData* frame)
{
    std::vector<uint8_t> extradata = buildExtraDataMp4(frame);
    if (extradata.empty())
        return;

    CodecParametersPtr context;
    if (frame->context)
    {
        context = std::make_shared<CodecParameters>(frame->context->getAvCodecParameters());
    }
    else
    {
        context = std::make_shared<CodecParameters>();
        context->getAvCodecParameters()->codec_type = AVMEDIA_TYPE_VIDEO;
        context->getAvCodecParameters()->codec_id = frame->compressionType;
    }
    context->setExtradata(extradata.data(), extradata.size());

    QSize streamResolution = nx::media::getFrameSize(frame);
    if (streamResolution.isEmpty())
        return;

    context->getAvCodecParameters()->width = streamResolution.width();
    context->getAvCodecParameters()->height = streamResolution.height();
    m_context = context;
}

QnCompressedVideoDataPtr AnnexbToMp4::process(const QnCompressedVideoData* frame)
{
    if (frame->compressionType != AV_CODEC_ID_H264 && frame->compressionType != AV_CODEC_ID_H265)
        return nullptr;

    if (isMp4Format(frame))
    {
        NX_VERBOSE(this, "Failed to convert from AnnexB to MP4 format, format is MP4 already");
        return nullptr;
    }

    if (frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
    {
        updateCodecParameters(frame);
    }

    if (!m_context)
    {
        NX_WARNING(this, "Failed to build codec parameters");
        return nullptr;
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
