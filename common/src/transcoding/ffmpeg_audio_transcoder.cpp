#include "ffmpeg_audio_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "core/datapacket/audio_data_packet.h"
#include "utils/media/audio_processor.h"


static const int MAX_AUDIO_JITTER = 1000 * 200;

QnFfmpegAudioTranscoder::QnFfmpegAudioTranscoder(CodecID codecId):
    QnAudioTranscoder(codecId),
    m_audioEncodingBuffer(0),
    m_resampleBuffer(0),
    m_encoderCtx(0),
    m_decoderContext(0),
    m_firstEncodedPts(AV_NOPTS_VALUE),
    m_lastTimestamp(AV_NOPTS_VALUE),
    m_downmixAudio(false),
    m_frameNum(0),
    m_resampleCtx(0)
{
    m_bitrate = 128*1000;
    m_audioEncodingBuffer = (quint8*) qMallocAligned(AVCODEC_MAX_AUDIO_FRAME_SIZE*2, 32);
    m_decodedBuffer =  (quint8*) qMallocAligned(AVCODEC_MAX_AUDIO_FRAME_SIZE*2, 32);
    m_decodedBufferSize = 0;
}

QnFfmpegAudioTranscoder::~QnFfmpegAudioTranscoder()
{
    qFreeAligned(m_audioEncodingBuffer);
    qFreeAligned(m_decodedBuffer);
    qFreeAligned(m_resampleBuffer);
    if (m_resampleCtx)
        audio_resample_close(m_resampleCtx);

    if (m_encoderCtx) {
        avcodec_close(m_encoderCtx);
        av_free(m_encoderCtx);
    }

    if (m_decoderContext) {
        avcodec_close(m_decoderContext);
        av_free(m_decoderContext);
    }

}

bool QnFfmpegAudioTranscoder::open(const QnConstCompressedAudioDataPtr& audio)
{
    if (!audio->context)
    {
        m_lastErrMessage = tr("Audio context was not specified.");
        return false;
    }

    QnAudioTranscoder::open(audio);

    return open(audio->context);
}

bool QnFfmpegAudioTranscoder::open(const QnMediaContextPtr& codecCtx)
{
    AVCodec* avCodec = avcodec_find_encoder(m_codecId);
    if (avCodec == 0)
    {
        m_lastErrMessage = tr("Could not find encoder for codec %1.").arg(m_codecId);
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(avCodec);
    //m_encoderCtx->codec_id = m_codecId;
    //m_encoderCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_encoderCtx->sample_fmt = avCodec->sample_fmts[0] != AV_SAMPLE_FMT_NONE ? avCodec->sample_fmts[0] : AV_SAMPLE_FMT_S16;

    m_encoderCtx->channels = codecCtx->ctx()->channels;
    if (m_encoderCtx->channels > 2 && (m_codecId == CODEC_ID_MP3 || m_codecId == CODEC_ID_MP2))
    {
        m_downmixAudio = true;
        m_encoderCtx->channels = 2;
    }
    m_encoderCtx->sample_rate = codecCtx->ctx()->sample_rate;
    if (m_encoderCtx->sample_rate < 16000) {
        m_encoderCtx->sample_rate = 16000;
    }

    m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    m_encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    m_encoderCtx->bit_rate = 64000 * m_encoderCtx->channels;


    if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize audio encoder.");
        return false;
    }

    avCodec = avcodec_find_decoder(codecCtx->ctx()->codec_id);
    m_decoderContext = avcodec_alloc_context3(0);
    avcodec_copy_context(m_decoderContext, codecCtx->ctx());
    if (avcodec_open2(m_decoderContext, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize audio decoder.");
        return false;
    }
    m_context = QnMediaContextPtr(new QnMediaContext(m_encoderCtx));
    m_frameNum = 0;
    return true;
}

int sampleSize(AVSampleFormat value)
{
    switch(value) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            return 1;

        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return 2;

        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return 4;

        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            return 8;
        default:
            return 2;
    }
}

bool QnFfmpegAudioTranscoder::existMoreData() const
{
    int encoderFrameSize = m_encoderCtx->frame_size * sampleSize(m_encoderCtx->sample_fmt) * m_encoderCtx->channels;
    return m_decodedBufferSize >= encoderFrameSize;
}

int QnFfmpegAudioTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    if( result )
        result->clear();

    if (!m_lastErrMessage.isEmpty())
        return -3;

    if (media) {
        if (qAbs(media->timestamp - m_lastTimestamp) > MAX_AUDIO_JITTER || (quint64)m_lastTimestamp == AV_NOPTS_VALUE)
        {
            m_frameNum = 0;
            m_firstEncodedPts = media->timestamp;
        }

        m_lastTimestamp = media->timestamp;
        QnConstCompressedAudioDataPtr audio = qSharedPointerDynamicCast<const QnCompressedAudioData>(media);
        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = const_cast<quint8*>((const quint8*)media->data());
        avpkt.size = media->dataSize();

        int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        // TODO: #vasilenko avoid using deprecated methods
        int len = avcodec_decode_audio3(m_decoderContext, (short *)(m_decodedBuffer + m_decodedBufferSize), &out_size, &avpkt);
        if (len < 0)
            return -3;
        if (out_size > 0) {
            quint8* srcData = m_decodedBuffer + m_decodedBufferSize;

            if (m_downmixAudio)
                out_size = QnAudioProcessor::downmix(srcData, out_size, m_decoderContext);
            if (m_encoderCtx->sample_rate != m_decoderContext->sample_rate ||
                m_encoderCtx->sample_fmt != m_decoderContext->sample_fmt)
            {
                if (m_resampleCtx == 0) 
                {
                    m_resampleCtx = av_audio_resample_init(
                        m_encoderCtx->channels,
                        m_decoderContext->channels,
                        m_encoderCtx->sample_rate,
                        m_decoderContext->sample_rate,
                        m_encoderCtx->sample_fmt,
                        m_decoderContext->sample_fmt,
                        16, 10, 0, 1.0);
                    m_resampleBuffer = (quint8*) qMallocAligned(AVCODEC_MAX_AUDIO_FRAME_SIZE*2, 32);
                }
                int inSamples = out_size / sampleSize(m_decoderContext->sample_fmt);
                out_size = audio_resample(m_resampleCtx, (short*) m_resampleBuffer, (short*) srcData, inSamples) * sampleSize(m_encoderCtx->sample_fmt);
                memcpy(srcData, m_resampleBuffer, out_size);
            }

            m_decodedBufferSize += out_size;
        }
        Q_ASSERT(m_decodedBufferSize < AVCODEC_MAX_AUDIO_FRAME_SIZE);
    }

    if( !result )
        return 0;

    int encoderFrameSize = m_encoderCtx->frame_size * sampleSize(m_encoderCtx->sample_fmt) * m_encoderCtx->channels;
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


    QnWritableCompressedAudioData* resultAudioPacket = new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, encoded, m_context);
    resultAudioPacket->compressionType = m_codecId;
    static AVRational r = {1, 1000000};
    //result->timestamp  = m_lastTimestamp;
    qint64 audioPts = m_frameNum++ * m_encoderCtx->frame_size;
    resultAudioPacket->timestamp  = av_rescale_q(audioPts, m_encoderCtx->time_base, r) + m_firstEncodedPts;
    resultAudioPacket->m_data.write((const char*) m_audioEncodingBuffer, encoded);
    *result = QnCompressedAudioDataPtr(resultAudioPacket);

    return 0;
}

AVCodecContext* QnFfmpegAudioTranscoder::getCodecContext()
{
    return m_encoderCtx;
}

#endif // ENABLE_DATA_PROVIDERS
