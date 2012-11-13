#include "ffmpeg_audio_transcoder.h"

static const int MAX_AUDIO_JITTER = 1000 * 200;

QnFfmpegAudioTranscoder::QnFfmpegAudioTranscoder(CodecID codecId):
QnAudioTranscoder(codecId),
m_encoderCtx(0),
m_decoderContext(0),
m_firstEncodedPts(AV_NOPTS_VALUE),
m_lastTimestamp(AV_NOPTS_VALUE)
{
    m_bitrate = 128*1000;
    m_audioEncodingBuffer = (quint8*) qMallocAligned(AVCODEC_MAX_AUDIO_FRAME_SIZE, 32);
    m_decodedBuffer =  (quint8*) qMallocAligned(AVCODEC_MAX_AUDIO_FRAME_SIZE*2, 32);
    m_decodedBufferSize = 0;
}

QnFfmpegAudioTranscoder::~QnFfmpegAudioTranscoder()
{
    qFreeAligned(m_audioEncodingBuffer);
    qFreeAligned(m_decodedBuffer);

    if (m_encoderCtx) {
        avcodec_close(m_encoderCtx);
        av_free(m_encoderCtx);
    }

    if (m_decoderContext) {
        avcodec_close(m_decoderContext);
        av_free(m_decoderContext);
    }

}

bool QnFfmpegAudioTranscoder::open(QnCompressedAudioDataPtr audio)
{
    if (!audio->context)
    {
        m_lastErrMessage = QObject::tr("Audio context must be specified");
        return false;
    }

    QnAudioTranscoder::open(audio);

    return open(audio->context);
}

bool QnFfmpegAudioTranscoder::open(QnMediaContextPtr codecCtx)
{
    AVCodec* avCodec = avcodec_find_encoder(m_codecId);
    if (avCodec == 0)
    {
        m_lastErrMessage = QObject::tr("Transcoder error: can't find encoder for codec %1").arg(m_codecId);
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(avCodec);
    //m_encoderCtx->codec_id = m_codecId;
    //m_encoderCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_encoderCtx->sample_fmt = AV_SAMPLE_FMT_S16; //avCodec->sample_fmts[0];
    m_encoderCtx->channels = codecCtx->ctx()->channels;
    m_encoderCtx->sample_rate = codecCtx->ctx()->sample_rate;

    m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    m_encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    m_encoderCtx->bit_rate = 64000 * m_encoderCtx->channels;

    //m_encoderCtx->channels = 2;
    //m_encoderCtx->sample_rate = 44100;
    //m_encoderCtx->bit_rate = 128000;

    if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = QObject::tr("Can't initialize audio encoder");
        return false;
    }

    avCodec = avcodec_find_decoder(codecCtx->ctx()->codec_id);
    m_decoderContext = avcodec_alloc_context3(0);
    avcodec_copy_context(m_decoderContext, codecCtx->ctx());
    if (avcodec_open2(m_decoderContext, avCodec, 0) < 0)
    {
        m_lastErrMessage = QObject::tr("Can't initialize audio decoder");
        return false;
    }
    m_context = QnMediaContextPtr(new QnMediaContext(m_encoderCtx));
    return true;
}

int QnFfmpegAudioTranscoder::transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result)
{
    result.clear();

    if (!m_lastErrMessage.isEmpty())
        return -3;

    if (media) {
        if (qAbs(media->timestamp - m_lastTimestamp) > MAX_AUDIO_JITTER || (quint64)m_lastTimestamp == AV_NOPTS_VALUE)
        {
            m_encoderCtx->frame_number = 0;
            m_firstEncodedPts = media->timestamp;
        }

        m_lastTimestamp = media->timestamp;
        QnCompressedAudioDataPtr audio = qSharedPointerDynamicCast<QnCompressedAudioData>(media);
        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = (quint8*)media->data.data();
        avpkt.size = media->data.size();

        int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        // TODO: #vasilenko avoid using deprecated methods
        int len = avcodec_decode_audio3(m_decoderContext, (short *)(m_decodedBuffer + m_decodedBufferSize), &out_size, &avpkt);
        if (len < 0)
            return -3;
        if (out_size > 0)
            m_decodedBufferSize += out_size;
        Q_ASSERT(m_decodedBufferSize < AVCODEC_MAX_AUDIO_FRAME_SIZE);
    }

    int encoderFrameSize = m_encoderCtx->frame_size*sizeof(short)*m_encoderCtx->channels;
    if (m_decodedBufferSize < encoderFrameSize)
        return 0;

    // TODO: #vasilenko avoid using deprecated methods
    int encoded = avcodec_encode_audio(m_encoderCtx, m_audioEncodingBuffer, FF_MIN_BUFFER_SIZE, (const short*) m_decodedBuffer);
    if (encoded < 0)
        return -3;

    memmove(m_decodedBuffer, m_decodedBuffer + encoderFrameSize, m_decodedBufferSize - encoderFrameSize);
    m_decodedBufferSize -= encoderFrameSize;

    if (encoded == 0)
        return 0;


    result = QnCompressedAudioDataPtr(new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, encoded, m_context));
    result->compressionType = m_codecId;
    static AVRational r = {1, 1000000};
    //result->timestamp  = m_lastTimestamp;
    qint64 audioPts = m_encoderCtx->frame_number*m_encoderCtx->frame_size;
    result->timestamp  = av_rescale_q(audioPts, m_encoderCtx->time_base, r) + m_firstEncodedPts;
    result->data.write((const char*) m_audioEncodingBuffer, encoded);

    return 0;
}

AVCodecContext* QnFfmpegAudioTranscoder::getCodecContext()
{
    return m_encoderCtx;
}
