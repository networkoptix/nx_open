// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"
#include <array>

#include <utils/media/jpeg_utils.h>
#include <utils/media/h263_utils.h>
#include <utils/media/h264_utils.h>
#include <utils/media/hevc_common.h>
#include <utils/media/hevc/extradata.h>
#include <utils/media/hevc/sequence_parameter_set.h>
#include <utils/media/mime_types.h>

namespace nx::media {

bool extractSps(const QnCompressedVideoData* videoData, hevc::SequenceParameterSet& sps)
{
    // H.265 nal units have same format (unit delimiter) as H.264 nal units
    auto nalUnits = nx::media::h264::decodeNalUnits(videoData); // TODO make another one for h265(in case of extradata format is differ).
    for (const auto& nalu: nalUnits)
    {
        hevc::NalUnitHeader packetHeader;
        if (!packetHeader.decode(nalu.data, nalu.size))
            return false;

        switch (packetHeader.unitType)
        {
            case hevc::NalUnitType::spsNut:
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
            hevc::SequenceParameterSet sps;
            if (extractSps(frame, sps))
                return QSize(sps.width, sps.height);
            return QSize();
        }
        case AV_CODEC_ID_H264:
        {
            SPSUnit sps;
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

bool fillExtraData(const QnCompressedVideoData* video, uint8_t** outExtradata, int* outSize)
{
    if (*outExtradata)
        return true;

    std::vector<uint8_t> extradata;
    if (video->compressionType == AV_CODEC_ID_H264)
        extradata = h264::buildExtraDataAnnexB((const uint8_t*)video->data(), video->dataSize());
    else if (video->compressionType == AV_CODEC_ID_H265)
        extradata = hevc::buildExtraDataAnnexB((const uint8_t*)video->data(), video->dataSize());
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
        return hevc::buildExtraDataAnnexB((const uint8_t*) frame->data(), frame->dataSize());
    return std::vector<uint8_t>();
}

QString fromVideoCodectoMimeType(AVCodecID codecId)
{
    switch (codecId)
    {
        case AV_CODEC_ID_H264:
            return kH264MimeType;
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

} // namespace nx::media
