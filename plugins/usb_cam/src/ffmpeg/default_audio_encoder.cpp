#include "default_audio_encoder.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"

#include <vector>

namespace nx {
namespace ffmpeg {

namespace {
    constexpr int kRecommendedStereoBitrate = 192000; // recommended stereo bitrate for mp3
}

std::unique_ptr<Codec> getDefaultMp3Encoder(int * outFfmpegError)
{
    auto encoder = std::make_unique<ffmpeg::Codec>();

    // int result = encoder->initializeEncoder(AV_CODEC_ID_MP3);
    // int result = encoder->initializeEncoder(AV_CODEC_ID_AAC);
    int result = encoder->initializeEncoder(AV_CODEC_ID_PCM_S16LE);
    if (result < 0)
    {
        *outFfmpegError = result;
        return nullptr;
    }

    AVCodecContext * context = encoder->codecContext();
    AVCodec * codec  = encoder->codec();

    context->sample_fmt = utils::suggestSampleFormat(codec);
    if (context->sample_fmt == AV_SAMPLE_FMT_NONE)
    {
        *outFfmpegError = AVERROR_ENCODER_NOT_FOUND;
        return nullptr;
    }

    context->sample_rate = utils::suggestSampleRate(codec);
    context->channel_layout = utils::suggestChannelLayout(codec);
    context->channels = av_get_channel_layout_nb_channels(context->channel_layout);

    /*!
     * formula found at: https://trac.ffmpeg.org/wiki/Encode/HighQualityAudio 
     */
    context->bit_rate = kRecommendedStereoBitrate * context->channels / 2;

    *outFfmpegError = 0;
    return encoder;
}

} // namespace ffmpeg
} // namespace nx