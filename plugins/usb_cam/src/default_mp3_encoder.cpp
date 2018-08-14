#include "default_mp3_encoder.h"
#include "ffmpeg/codec.h"

#include <vector>

namespace nx {
namespace usb_cam {

namespace {

AVSampleFormat suggestSampleFormat(AVCodec * codec)
{
    static const std::vector<AVSampleFormat> priorityList = 
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

    for(const auto & sampleFormat : priorityList)
    {
        const AVSampleFormat * format = codec->sample_fmts;
        for(; format && *format != AV_SAMPLE_FMT_NONE; ++format)
        {
            if(*format == sampleFormat)
                return *format;
        }
    }
    return AV_SAMPLE_FMT_NONE;
}

int suggestSampleRate(AVCodec * codec)
{
    if (!codec->supported_samplerates)
        return 44100;

    int largest = 0;
    const int * sampleRate = codec->supported_samplerates;
    for(; *sampleRate; ++sampleRate)
        largest = FFMAX(largest, *sampleRate);
    return largest;
}

int suggestChannelLayout(AVCodec * codec)
{
    if (!codec->channel_layouts)
        return AV_CH_LAYOUT_STEREO;

    uint64_t bestLayout = 0;
    int bestNbChannels = 0;

    const uint64_t *layout = codec->channel_layouts;
    for (; *layout; ++layout)
     {
        int nbChannels = av_get_channel_layout_nb_channels(*layout);
        if (nbChannels > bestNbChannels) 
        {
            bestLayout   = *layout;
            bestNbChannels = nbChannels;
        }
    }
    return bestLayout;
}

}

std::unique_ptr<ffmpeg::Codec> getDefaultMp3Encoder(int * outFfmpegError)
{
    auto encoder = std::make_unique<ffmpeg::Codec>();

    int result = encoder->initializeEncoder("libmp3lame");
    if (result < 0)
    {
        encoder = std::make_unique<ffmpeg::Codec>();
        result = encoder->initializeEncoder(AV_CODEC_ID_MP3);
        if (result < 0)
        {
            *outFfmpegError = result;
            return nullptr;
        }
    }

    AVCodecContext * context = encoder->codecContext();
    AVCodec * codec  = encoder->codec();

    context->sample_fmt = suggestSampleFormat(codec);
    if(context->sample_fmt == AV_SAMPLE_FMT_NONE)
    {
        *outFfmpegError = AVERROR_ENCODER_NOT_FOUND;
        return nullptr;
    }

    context->sample_rate = suggestSampleRate(codec);
    context->channel_layout = suggestChannelLayout(codec);
    context->channels = av_get_channel_layout_nb_channels(context->channel_layout);
    context->bit_rate = 64000;

    *outFfmpegError = 0;
    return encoder;
    }

} // namespace nx
} // namespace usb_cam