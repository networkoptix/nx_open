// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <array>

#include <nx/codec/h263/h263_utils.h>
#include <nx/codec/h265/extradata.h>
#include <nx/codec/h265/hevc_common.h>
#include <nx/codec/h265/hevc_decoder_configuration_record.h>
#include <nx/codec/h265/sequence_parameter_set.h>
#include <nx/codec/jpeg/jpeg_utils.h>
#include <nx/media/h264_utils.h>
#include <nx/media/mime_types.h>
#include <nx/utils/log/log.h>

namespace nx::media {

bool isExtradataInMp4Format(const QnCompressedVideoData* data)
{
    return data->context &&
        data->context->getExtradataSize() >= h265::HEVCDecoderConfigurationRecord::kMinSize &&
        data->context->getExtradata()[0] == 1; // configurationVersion
}

bool extractSpsH265(const QnCompressedVideoData* data, h265::SequenceParameterSet& sps)
{
    std::vector<nal::NalUnitInfo> nalUnits;
    if (isExtradataInMp4Format(data))
    {
        h265::HEVCDecoderConfigurationRecord record;
        if (!record.read(data->context->getExtradata(), data->context->getExtradataSize()))
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed to parse H265 extradata");
            return false;
        }
        if (record.sps.empty())
        {
            NX_DEBUG(NX_SCOPE_TAG, "No SPS in extradata");
            return false;
        }
        return parseNalUnit(record.sps[0].data(), record.sps[0].size(), sps);
    }

    nalUnits = nal::findNalUnitsAnnexB((const uint8_t*)data->data(), data->dataSize());
    for (const auto& nalu: nalUnits)
    {
        h265::NalUnitHeader packetHeader;
        if (!packetHeader.decode(nalu.data, nalu.size))
            return false;

        switch (packetHeader.unitType)
        {
            case h265::NalUnitType::spsNut:
                return parseNalUnit(nalu.data, nalu.size, sps);
            default:
                break;
        }
    }
    return false;
}

QSize getFrameSize(const QnCompressedVideoData* frame)
{
    // If we use fast method to find stream info, ffmpeg can get 8x8 resolution.
    constexpr int kInvalidFrameSize = 8;

    if (frame->width > kInvalidFrameSize && frame->height > kInvalidFrameSize)
        return QSize(frame->width, frame->height);

    switch (frame->compressionType)
    {
        case AV_CODEC_ID_H265:
        {
            h265::SequenceParameterSet sps;
            if (extractSpsH265(frame, sps))
                return QSize(sps.width, sps.height);
            return QSize();
        }
        case AV_CODEC_ID_H264:
        {
            h264::SequenceParameterSet sps;
            if (h264::extractSps(frame, sps))
                return QSize(sps.getWidth(), sps.getHeight());
            return QSize();
        }
        case AV_CODEC_ID_MJPEG:
        {
            nx::media::jpeg::ImageInfo imgInfo;
            nx::media::jpeg::readJpegImageInfo((const quint8*) frame->data(), frame->dataSize(), &imgInfo);
            return QSize(imgInfo.width, imgInfo.height);
        }
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
        {
            h263::PictureHeader header;
            if (header.decode((uint8_t*)frame->data(), frame->dataSize()))
                return QSize(header.width, header.height);

            return QSize();
        }
        // TODO support mpeg2video header(VMS-20363)
        default:
            return QSize();
    }
}

double getDefaultSampleAspectRatio(const QSize& srcSize)
{
    struct AspectRatioData
    {
        AspectRatioData() {}
        AspectRatioData(const QSize& srcSize, const QSize& dstAspect) :
            srcSize(srcSize),
            dstAspect(dstAspect)
        {
        }

        QSize srcSize;
        QSize dstAspect;
    };

    static std::array<AspectRatioData, 6> kOverridedAr =
    {
        AspectRatioData(QSize(720, 576), QSize(4, 3)),
        AspectRatioData(QSize(720, 480), QSize(4, 3)),
        AspectRatioData(QSize(720, 240), QSize(4, 3)),
        AspectRatioData(QSize(704, 576), QSize(4, 3)),
        AspectRatioData(QSize(704, 480), QSize(4, 3)),
        AspectRatioData(QSize(704, 240), QSize(4, 3))
    };

    for (const auto& data: kOverridedAr)
    {
        if (data.srcSize == srcSize)
        {
            double dstAspect = (double) data.dstAspect.width() / data.dstAspect.height();
            double srcAspect = (double) srcSize.width() / srcSize.height();
            return dstAspect / srcAspect;
        }
    }

    return 1.0;
}

bool fillExtraDataAnnexB(const QnCompressedVideoData* video, uint8_t** outExtradata, int* outSize)
{
    if (*outExtradata)
        return true;

    std::vector<uint8_t> extradata;
    if (video->compressionType == AV_CODEC_ID_H264)
        extradata = h264::buildExtraDataAnnexB((const uint8_t*)video->data(), video->dataSize());
    else if (video->compressionType == AV_CODEC_ID_H265)
        extradata = h265::buildExtraDataAnnexB((const uint8_t*)video->data(), video->dataSize());
    else
        return true; //< at this moment ignore all other codecs

    if (extradata.empty())
        return false;

    *outExtradata = (uint8_t*)av_mallocz(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!*outExtradata)
        return false;
    *outSize = extradata.size();
    memcpy(*outExtradata, extradata.data(), extradata.size());
    return true;
}

std::vector<uint8_t> buildExtraDataAnnexB(const QnConstCompressedVideoDataPtr& frame)
{
    if (frame->context && frame->context->getExtradata())
    {
        return std::vector<uint8_t>(
            frame->context->getExtradata(),
            frame->context->getExtradata() + frame->context->getExtradataSize());
    }

    if (frame->compressionType == AV_CODEC_ID_H264)
        return h264::buildExtraDataAnnexB((const uint8_t*) frame->data(), frame->dataSize());
    else if (frame->compressionType == AV_CODEC_ID_H265)
        return h265::buildExtraDataAnnexB((const uint8_t*) frame->data(), frame->dataSize());
    return std::vector<uint8_t>();
}

QString fromVideoCodectoMimeType(AVCodecID codecId)
{
    switch (codecId)
    {
        case AV_CODEC_ID_H264:
            return kH264MimeType;
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
            return kH263MimeType;
        case AV_CODEC_ID_HEVC:
            return kHevcMimeType;
        case AV_CODEC_ID_MPEG4:
            return kMpeg4MimeType;
        case AV_CODEC_ID_MJPEG:
            return kMjpegMimeType;
        default:
            NX_ASSERT(false, "Unsupported video codec id %1", codecId);
            return QString();
    }
}

AVCodecID fromMimeTypeToVideoCodec(const std::string& mime)
{
    if (mime == kH264MimeType)
        return AV_CODEC_ID_H264;
    else if (mime == kH263MimeType)
        return AV_CODEC_ID_H263;
    else if (mime == kHevcMimeType)
        return AV_CODEC_ID_HEVC;
    else if (mime == kMjpegMimeType)
        return AV_CODEC_ID_MJPEG;
    else if (mime == kMpeg4MimeType)
        return AV_CODEC_ID_MPEG4;
    else
        return AV_CODEC_ID_NONE;
}

} // namespace nx::media
