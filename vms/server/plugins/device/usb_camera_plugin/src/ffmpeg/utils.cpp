#include "utils.h"

#include <vector>

#include <camera/camera_plugin.h>

namespace nx {
namespace usb_cam {
namespace ffmpeg{
namespace utils{

namespace {

static int greatestCommonDenominator(int n1, int n2)
{
    int gcd = 1;
    for (int i = 1; i <= n1 && i <= n2; ++i)
    {
        if (n1 % i == 0 && n2 % i == 0)
            gcd = i;
    }
    return gcd;
}

} // namespace

std::string errorToString(int errorCode)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errorCode, errorBuffer, AV_ERROR_MAX_STRING_SIZE);
    return errorBuffer;
}

std::string codecIdToName(AVCodecID codecId)
{
   return avcodec_get_name(codecId);
}

AVCodecID codecNameToId(const char * codecName)
{
    AVCodec* codec = avcodec_find_decoder_by_name(codecName);
    if (codec)
        return codec->id;
    codec = avcodec_find_encoder_by_name(codecName);
    if (codec)
        return codec->id;
    return AV_CODEC_ID_NONE;
}

AVPixelFormat suggestPixelFormat(const AVCodec* codec)
{
    if (codec)
        return codec->pix_fmts ? codec->pix_fmts[0] : AV_PIX_FMT_NONE;
    return AV_PIX_FMT_NONE;
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

nxcip::DataPacketType toNxDataPacketType(AVMediaType mediaType)
{
    switch(mediaType)
    {
        case AVMEDIA_TYPE_VIDEO:
            return nxcip::dptVideo;
        case AVMEDIA_TYPE_AUDIO:
            return nxcip::dptAudio;
        default:
            return nxcip::dptEmpty;
    }
}

AVMediaType toAVMediaType(nxcip::DataPacketType mediaType)
{
    switch(mediaType)
    {
        case nxcip::dptVideo:
            return AVMEDIA_TYPE_VIDEO;
        case nxcip::dptAudio:
            return AVMEDIA_TYPE_AUDIO;
        default:
            return AVMEDIA_TYPE_UNKNOWN;
    }
}

nxcip::CompressionType toNxCompressionType(AVCodecID codecId)
{
    switch (codecId)
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

AVCodecID toAVCodecId(nxcip::CompressionType codecId)
{
    switch (codecId)
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

std::optional<nxcip::AudioFormat::SampleType> toNxSampleType(AVSampleFormat format)
{
    switch (format)
    {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            return nxcip::AudioFormat::stU8;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return nxcip::AudioFormat::stS16;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            return nxcip::AudioFormat::stS32;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return nxcip::AudioFormat::stFLT;
        default:
            return std::optional<nxcip::AudioFormat::SampleType>();
    }
}

uint64_t suggestChannelLayout(const AVCodec *codec)
{
    if (!codec->channel_layouts)
        return AV_CH_LAYOUT_STEREO;

    uint64_t bestLayout = 0;
    int bestNbChannels = 0;
    for (const uint64_t *channel = codec->channel_layouts; *channel; ++channel)
    {
        int nbChannels = av_get_channel_layout_nb_channels(*channel);
        if (nbChannels > bestNbChannels)
        {
            bestLayout = *channel;
            bestNbChannels = nbChannels;
        }
    }
    return bestLayout;
}

AVSampleFormat suggestSampleFormat(const AVCodec * codec)
{
    static const std::vector<AVSampleFormat> kPriorityList = 
    {
        AV_SAMPLE_FMT_S32,
        AV_SAMPLE_FMT_S16,
        AV_SAMPLE_FMT_FLT,
        AV_SAMPLE_FMT_U8,
        AV_SAMPLE_FMT_S32P,
        AV_SAMPLE_FMT_S16P,
        AV_SAMPLE_FMT_FLTP,
        AV_SAMPLE_FMT_U8P
    };

    for (const auto & sampleFormat : kPriorityList)
    {
        const AVSampleFormat * format = codec->sample_fmts;
        for (; format && *format != AV_SAMPLE_FMT_NONE; ++format)
        {
            if (*format == sampleFormat)
                return *format;
        }
    }
    return AV_SAMPLE_FMT_NONE;
}

int suggestSampleRate(const AVCodec * codec)
{
    static constexpr int kDefaultSampleRate = 44100;

    if (!codec->supported_samplerates)
        return kDefaultSampleRate;

    int largest = 0;
    for (const int * sampleRate = codec->supported_samplerates; *sampleRate; ++sampleRate)
        largest = FFMAX(largest, *sampleRate);
    return largest;
}

AVRational toFraction(float number, int precision)
{
    int wholeNumber = (int) number;
    float decimal = number - wholeNumber;

    if (!decimal)
        return {(int) number, 1};

    int numerator = decimal * precision;
    int denominator = precision;

    int gcd;
    do
    {
        gcd = greatestCommonDenominator(numerator, denominator);
        numerator /= gcd;
        denominator /= gcd;
    }while (gcd != 1);

    if (wholeNumber > 0)
        numerator = denominator * wholeNumber + numerator;

    return {numerator, denominator};
}

} // namespace utils
} // namespace ffmpeg
} // namespace usb_cam
} // namesapce nx
