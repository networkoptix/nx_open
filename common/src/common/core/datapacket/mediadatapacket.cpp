#include "mediadatapacket.h"

#include "utils/ffmpeg/ffmpeg_global.h"

void CLCodecAudioFormat::fromAvStream(AVCodecContext* c)
{
    if (c->sample_rate)
        setFrequency(c->sample_rate);

    if (c->channels)
        setChannels(c->channels);

    //setCodec("audio/pcm");
    setByteOrder(QAudioFormat::LittleEndian);

    switch(c->sample_fmt)
    {
    case SAMPLE_FMT_U8: ///< unsigned 8 bits
        setSampleSize(8);
        setSampleType(QAudioFormat::UnSignedInt);
        break;

    case SAMPLE_FMT_S16: ///< signed 16 bits
        setSampleSize(16);
        setSampleType(QAudioFormat::SignedInt);
        break;

    case SAMPLE_FMT_S32:///< signed 32 bits
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

    if (c->extradata_size > 0)
    {
        extraData.resize(c->extradata_size);
        memcpy(&extraData[0], c->extradata, c->extradata_size);
    }
    bitrate = c->bit_rate;
    channel_layout = c->channel_layout;
    block_align = c->block_align;
}
