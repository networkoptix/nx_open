#include "audio_processor.h"

static inline short clip_short(int v)
{
    if (v < -32768)
        return -32768;
    else if (v > 32767)
        return 32767;
    else
        return (short) v;
}

template <class T>
static void down_mix_to_stereo(T *data, int channels, int len)
{
    T* output = data;
    T* input = data;

    int steps = len / 6 / sizeof(T);

    for(int i = 0; i < steps; ++i)
    {
        /* 5.1 to stereo. l, c, r, ls, rs, sw */
        int fl,fr,c,rl,rr,lfe;

        fl = input[0];
        fr = input[1];
        if (channels >= 6)
        {
            c =  input[2];
            lfe = input[3];
            rl = input[4];
            rr = input[5];

            /* Postings on Doom9 say that Dolby specifically says the LFE (.1)
             * channel should usually be ignored during downmixing to Dolby ProLogic II,
             * with quotes from official Dolby documentation. */
            (void)lfe;

            output[0] = clip_short(fl + (0.5 * rl) + (0.7 * c));
            output[1] = clip_short(fr + (0.5 * rr) + (0.7 * c));
        }
        else
        {
            output[0] = fl;
            output[1] = fr;
        }
        output += 2;
        input += channels;
    }
}

QnCodecAudioFormat QnAudioProcessor::downmix(QnByteArray& audio, QnCodecAudioFormat format)
{
    if (format.channelCount() > 2)
    {
        if (format.sampleSize() == 8)
            down_mix_to_stereo<qint8>((qint8*)(audio.data()), format.channelCount(), audio.size());
        else if (format.sampleSize() == 16)
            down_mix_to_stereo<qint16>((qint16*)(audio.data()), format.channelCount(), audio.size());
        else if (format.sampleSize() == 32)
            down_mix_to_stereo<qint32>((qint32*)(audio.data()), format.channelCount(), audio.size());
        else
        {
            NX_ASSERT(1 == 0, Q_FUNC_INFO + __LINE__, "invalid sample size");
        }
        audio.resize(audio.size() / format.channelCount() * 2);
        format.setChannelCount(2);
    }
    return format;
}

QnCodecAudioFormat QnAudioProcessor::float2int16(QnByteArray& audio, QnCodecAudioFormat format)
{
    NX_ASSERT(sizeof(float) == 4); // not sure about sizeof(float) in 64 bit version

    qint32* inP = (qint32*)(audio.data());
    qint16* outP = (qint16*)inP;
    int len = audio.size()/4;

    for (int i = 0; i < len; ++i)
    {
        float f = *((float*)inP);
        *outP = clip_short((qint32)(f * (1 << 15)));
        ++inP;
        ++outP;
    }
    audio.resize(audio.size() / 2);
    format.setSampleSize(16);
    format.setSampleType(QnAudioFormat::SignedInt);
    return format;
}

QnCodecAudioFormat QnAudioProcessor::int32Toint16(QnByteArray& audio, QnCodecAudioFormat format)
{
    NX_ASSERT(sizeof(float) == 4); // not sure about sizeof(float) in 64 bit version

    qint32* inP = (qint32*)(audio.data());
    qint16* outP = (qint16*)inP;
    int len = audio.size()/4;

    for (int i = 0; i < len; ++i)
    {
        *outP = *inP >> 16;
        ++inP;
        ++outP;
    }
    audio.resize(audio.size() / 2);
    format.setSampleSize(16);
    format.setSampleType(QnAudioFormat::SignedInt);
    return format;
}

QnCodecAudioFormat QnAudioProcessor::float2int32(QnByteArray& audio, QnCodecAudioFormat format)
{
    NX_ASSERT(sizeof(float) == 4); // not sure about sizeof(float) in 64 bit version

    qint32* inP = (qint32*)(audio.data());
    int len = audio.size()/4;
    for (int i = 0; i < len; ++i)
    {
        float f = *((float*)inP);
        *inP = (qint32) (f * INT_MAX);
        ++inP;
    }
    format.setSampleType(QnAudioFormat::SignedInt);
    return format;
}
