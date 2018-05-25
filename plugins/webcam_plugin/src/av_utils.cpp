#include "av_utils.h"

#include <camera/camera_plugin.h>

namespace nx {
namespace webcam_plugin {
namespace utils{
namespace av{

std::string avStrError(int errorCode)
{
    const int length = AV_ERROR_MAX_STRING_SIZE;
    char errorBuffer[length];
    av_strerror(errorCode, errorBuffer, length);
    return std::string(errorBuffer);
}

AVStream* getAVStream(AVFormatContext * context, AVMediaType mediaType, int * streamIndex)
{
    if(!context)
        return nullptr;

    for (unsigned int i = 0; i < context->nb_streams; ++i)
    {
        if(!context->streams[i] || !context->streams[i]->codecpar)
            continue;

        if (context->streams[i]->codecpar->codec_type == mediaType)
        {
            if(streamIndex)
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
        case AV_CODEC_ID_MPEG2VIDEO:
            return nxcip::AV_CODEC_ID_MPEG2VIDEO;
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
        case AV_CODEC_ID_H263I:
            return nxcip::AV_CODEC_ID_H263;
        case AV_CODEC_ID_MJPEG:
            return nxcip::AV_CODEC_ID_MJPEG;
        case AV_CODEC_ID_MPEG4:
            return nxcip::AV_CODEC_ID_MPEG4;
        case AV_CODEC_ID_H264:
            return nxcip::AV_CODEC_ID_H264;
        case  AV_CODEC_ID_THEORA:
            return nxcip::AV_CODEC_ID_THEORA;
        case AV_CODEC_ID_PNG:
            return nxcip::AV_CODEC_ID_PNG;
        case AV_CODEC_ID_GIF:
            return nxcip::AV_CODEC_ID_GIF;
        case AV_CODEC_ID_MP2:
            return nxcip::AV_CODEC_ID_MP2;
        case AV_CODEC_ID_MP3:
            return nxcip::AV_CODEC_ID_MP3;
        case AV_CODEC_ID_AAC:
            return nxcip::AV_CODEC_ID_AAC;
        case AV_CODEC_ID_AC3:
            return nxcip::AV_CODEC_ID_AC3;
        case AV_CODEC_ID_DTS:
            return nxcip::AV_CODEC_ID_DTS;
        case AV_CODEC_ID_PCM_S16LE:
            return nxcip::AV_CODEC_ID_PCM_S16LE;
        case AV_CODEC_ID_PCM_MULAW:
            return nxcip::AV_CODEC_ID_PCM_MULAW;
        case AV_CODEC_ID_VORBIS:
            return nxcip::AV_CODEC_ID_VORBIS;
        case AV_CODEC_ID_NONE:
        default:
            return nxcip::AV_CODEC_ID_NONE;
    }
}

    AVCodecID toAVCodecID(nxcip::CompressionType codecID)
    {
        switch (codecID)
        {
            case nxcip::AV_CODEC_ID_MPEG2VIDEO:
                return AV_CODEC_ID_MPEG2VIDEO;
            case nxcip::AV_CODEC_ID_H263:
                return AV_CODEC_ID_H263P;
            case nxcip::AV_CODEC_ID_MJPEG:
                return AV_CODEC_ID_MJPEG;
            case nxcip::AV_CODEC_ID_MPEG4:
                return AV_CODEC_ID_MPEG4;
            case nxcip::AV_CODEC_ID_H264:
                return AV_CODEC_ID_H264;
            case nxcip::AV_CODEC_ID_THEORA:
                return AV_CODEC_ID_THEORA;
            case nxcip::AV_CODEC_ID_PNG:
                return AV_CODEC_ID_PNG;
            case nxcip::AV_CODEC_ID_GIF:
                return AV_CODEC_ID_GIF;
            case nxcip::AV_CODEC_ID_MP2:
                return AV_CODEC_ID_MP2;
            case nxcip::AV_CODEC_ID_MP3:
                return AV_CODEC_ID_MP3;
            case nxcip::AV_CODEC_ID_AAC:
                return AV_CODEC_ID_AAC;
            case nxcip::AV_CODEC_ID_AC3:
                return AV_CODEC_ID_AC3;
            case nxcip::AV_CODEC_ID_DTS:
                return AV_CODEC_ID_DTS;
            case nxcip::AV_CODEC_ID_PCM_S16LE:
                return AV_CODEC_ID_PCM_S16LE;
            case nxcip::AV_CODEC_ID_PCM_MULAW:
                return AV_CODEC_ID_PCM_MULAW;
            case nxcip::AV_CODEC_ID_VORBIS:
                return AV_CODEC_ID_VORBIS;
            case nxcip::AV_CODEC_ID_NONE:
            default:
                return AV_CODEC_ID_NONE;
        }
    }


} // namespace av
} // namespace utils
} // namespace webcam_plugin
} // namesapce nx
