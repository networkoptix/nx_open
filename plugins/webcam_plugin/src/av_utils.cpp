#include "StdAfx.h"
#include "av_utils.h"

namespace utils{
namespace av{

    AVStream* getAvStream(AVFormatContext * context, int * streamIndex, enum AVMediaType mediaType)
    {
        for (unsigned int i = 0; i < context->nb_streams; ++i)
        {
            if (context->streams[i]->codecpar->codec_type == mediaType)
            {
                *streamIndex = i;
                return context->streams[i];
            }
        }

        return nullptr;
    }

    AVPixelFormat suggestPixelFormat(AVCodecID codecID)
    {
        switch (codecID)
        {
            case AV_CODEC_ID_H264:
                return AV_PIX_FMT_YUV444P;
            case AV_CODEC_ID_MJPEG:
                return AV_PIX_FMT_YUVJ420P;
            default:
                return AV_PIX_FMT_YUV420P;
        }
    }

    AVPixelFormat unDeprecatePixelFormat(AVPixelFormat pixelFormat)
    {
        switch (pixelFormat)
        {
            case AV_PIX_FMT_YUVJ420P:
                return AV_PIX_FMT_YUV420P;
            case AV_PIX_FMT_YUVJ422P:
                return AV_PIX_FMT_YUV422P;
            case AV_PIX_FMT_YUVJ444P:
                return AV_PIX_FMT_YUV444P;
            case AV_PIX_FMT_YUVJ440P:
                return AV_PIX_FMT_YUV440P;
            default:
                return pixelFormat;
        }
    }

    nxcip::CompressionType toNxCompressionType(AVCodecID codecID)
    {
        switch (codecID)
        {
            case AV_CODEC_ID_H264:
                return nxcip::AV_CODEC_ID_H264;
            case AV_CODEC_ID_MJPEG:
            default:
                return nxcip::AV_CODEC_ID_MJPEG;
        }
    }

} // namespace av
} // namespace utils
