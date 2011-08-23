#include "mediadatapacket.h"

#include "utils/ffmpeg/ffmpeg_global.h"

void CLCodecAudioFormat::fromAvStream(AVCodecContext *context)
{
    if (!context)
        return;

    if (context->sample_rate)
        setFrequency(context->sample_rate);

    if (context->channels)
        setChannels(context->channels);

    //setCodec("audio/pcm");
    setByteOrder(QAudioFormat::LittleEndian);

    switch (context->sample_fmt)
    {
    case SAMPLE_FMT_U8: // unsigned 8 bits
        setSampleSize(8);
        setSampleType(QAudioFormat::UnSignedInt);
        break;

    case SAMPLE_FMT_S16: // signed 16 bits
        setSampleSize(16);
        setSampleType(QAudioFormat::SignedInt);
        break;

    case SAMPLE_FMT_S32: // signed 32 bits
        setSampleSize(32);
        setSampleType(QAudioFormat::SignedInt);
        break;

    case AV_SAMPLE_FMT_FLT:
        setSampleSize(32);
        setSampleType(QAudioFormat::Float);
        break;

    default:
        break;
    }

    if (context->extradata_size > 0)
    {
        extraData.resize(context->extradata_size);
        memcpy(extraData.data(), context->extradata, context->extradata_size);
    }
    bitrate = context->bit_rate;
    channel_layout = context->channel_layout;
    block_align = context->block_align;
}
