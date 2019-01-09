#include "default_audio_encoder.h"

#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr int kDefaultAudioBitrate = 128000;
static constexpr AVCodecID kDefaultAudioCodecId = AV_CODEC_ID_AAC;

} //namespace 

std::unique_ptr<ffmpeg::Codec> getDefaultAudioEncoder(int * outFfmpegError)
{
    auto encoder = std::make_unique<ffmpeg::Codec>();

    int result = encoder->initializeEncoder(kDefaultAudioCodecId);
    if (result < 0)
    {
        *outFfmpegError = result;
        return nullptr;
    }

    AVCodecContext * context = encoder->codecContext();
    const AVCodec * codec = encoder->codec();

    context->sample_fmt = ffmpeg::utils::suggestSampleFormat(codec);
    if (context->sample_fmt == AV_SAMPLE_FMT_NONE)
    {
        *outFfmpegError = AVERROR_ENCODER_NOT_FOUND;
        return nullptr;
    }

    context->sample_rate = ffmpeg::utils::suggestSampleRate(codec);
    context->channel_layout = ffmpeg::utils::suggestChannelLayout(codec);
    context->channels = av_get_channel_layout_nb_channels(context->channel_layout);
    context->time_base = {1, context->sample_rate};
    context->codec_type = AVMEDIA_TYPE_AUDIO;
    int recommendedStereoBitrate = kDefaultAudioBitrate;

    // Formula found at: https://trac.ffmpeg.org/wiki/Encode/HighQualityAudio
    context->bit_rate = recommendedStereoBitrate * context->channels / 2;

    *outFfmpegError = 0;
    return encoder;
}

} // namespace usb_cam
} // namespace nx
