#include "ffmpegaudiodecoder_p.h"

#include <QtMultimedia/QAudioFormat>

#include "core/datapacket/mediadatapacket.h"
#include "utils/ffmpeg/ffmpeg_global.h"
#include "utils/common/aligned_data.h"
#include "utils/common/log.h"

#define INBUF_SIZE 4096

static inline AVSampleFormat audioFormatQtToFFmpeg(const QAudioFormat &fmt)
{
    const int ss = fmt.sampleSize();
    const QAudioFormat::SampleType st = fmt.sampleType();
    if (ss == 8)
        return AV_SAMPLE_FMT_U8;
    if (ss == 16 && st == QAudioFormat::SignedInt)
        return AV_SAMPLE_FMT_S16;
    if (ss == 32 && st == QAudioFormat::SignedInt)
        return AV_SAMPLE_FMT_S32;
    if (ss == 32 && st == QAudioFormat::Float)
        return AV_SAMPLE_FMT_FLT;
    return AV_SAMPLE_FMT_NONE;
}


CLFFmpegAudioDecoder::CLFFmpegAudioDecoder(QnCompressedAudioDataPtr data)
    : m_codec(0), m_context(0)
{
    QnFFmpeg::initialize();

    CodecID codecId = CodecID(data->compressionType);
    m_codec = QnFFmpeg::findDecoder(codecId);
    if (m_codec)
    {
        m_context = avcodec_alloc_context();

        m_context->codec_id = codecId;
        m_context->codec_type = AVMEDIA_TYPE_AUDIO;
        m_context->sample_fmt = audioFormatQtToFFmpeg(data->format);
        m_context->channels = data->format.channels();
        m_context->sample_rate = data->format.frequency();
        m_context->bits_per_coded_sample = qMin(data->format.sampleSize(), 24);

        m_context->bit_rate = data->format.bitrate;
        m_context->block_align = data->format.block_align;
        m_context->channel_layout = data->format.channel_layout;

        if (data->format.extraData.size() > 0)
        {
            m_context->extradata_size = data->format.extraData.size();
            m_context->extradata = (quint8 *)av_malloc(m_context->extradata_size);
            memcpy(m_context->extradata, data->format.extraData.constData(), m_context->extradata_size);
        }

        if (!QnFFmpeg::openCodec(m_context, m_codec))
        {
            QnFFmpeg::closeCodec(m_context);
            av_free(m_context);
            m_context = 0;

            setError(CodecContextError);
            cl_log.log(QLatin1String("CLFFmpegAudioDecoder::CLFFmpegAudioDecoder(): could not open codec!"), cl_logWARNING);
        }
    }
    else
    {
        setError(CodecError);
        cl_log.log(QLatin1String("CLFFmpegAudioDecoder::CLFFmpegAudioDecoder(): codec not found!"), cl_logWARNING);
    }
}

CLFFmpegAudioDecoder::~CLFFmpegAudioDecoder()
{
    if (m_context)
    {
        QnFFmpeg::closeCodec(m_context);
        av_free(m_context);
    }
}

//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than
//the actual read bytes because some optimized bit stream readers
//read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure
//that no over reading happens for damaged MPEG streams.
bool CLFFmpegAudioDecoder::decode(CLAudioData &data)
{
    if (!m_context)
        return false;

    const uchar *inbuf_ptr = data.inbuf;
    int size = data.inbuf_len;
    uchar *outbuf = (uchar *)data.outbuf->data();

    data.outbuf_len = 0;

    while (size > 0)
    {
        int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        if (data.outbuf_len + out_size > data.outbuf->capacity())
        {
            if (data.outbuf->capacity() >= (unsigned long)MAX_AUDIO_FRAME_SIZE) // can not increase capacity any more
            {
                Q_ASSERT(false);
                data.outbuf_len = 0;
                outbuf = (uchar *)data.outbuf->data(); // start form beginning
            }
            else
            {
                unsigned long new_capacity = qMin((unsigned long)MAX_AUDIO_FRAME_SIZE, (unsigned long)(data.outbuf->capacity() * 1.5));
                new_capacity = qMax(new_capacity, data.outbuf_len + out_size);
                data.outbuf->change_capacity(new_capacity);
                outbuf = (uchar *)data.outbuf->data() + data.outbuf_len;
            }
        }

        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = (quint8*)inbuf_ptr;
        avpkt.size = size;

        const int len = avcodec_decode_audio3(m_context, (short *)outbuf, &out_size, &avpkt);
        if (len < 0)
        {
            setError(FrameDecodeError);
            return false;
        }

        data.outbuf_len += out_size;
        outbuf += out_size;
        size -= len;
        inbuf_ptr += len;
    }

    //audioCodecFillFormat(data.format, m_context);

    setError(NoError);

    return true;
}
