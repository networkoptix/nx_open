#include "ffmpeg_audio_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>
#include "utils/media/audio_processor.h"
#include "utils/media/ffmpeg_helper.h"

namespace
{
    static const int MAX_AUDIO_JITTER = 1000 * 200;

    int calcBits(quint64 value)
    {
        int result = 0;
        while (value)
        {
            if (value & 1)
                ++result;
            value >>= 1;
        }
        return result;
    }

    int getMaxAudioChannels(AVCodec* avCodec)
    {
        if (!avCodec->channel_layouts)
            return 1; // default value if unknown

        int result = 0;
        for (const uint64_t* layout = avCodec->channel_layouts; *layout; ++layout)
            result = qMax(result, calcBits(*layout));
        return result;
    }

    int getDefaultDstSampleRate(int srcSampleRate, AVCodec* avCodec)
    {
        int result = srcSampleRate;
        if (avCodec->id == AV_CODEC_ID_ADPCM_G726 ||
            avCodec->id == AV_CODEC_ID_PCM_MULAW ||
            avCodec->id == AV_CODEC_ID_PCM_ALAW)
        {
            result = 8000;
        }
        else
        {
            result = qMax(result, 16000);
        }
        return result;
    }
}

QnFfmpegAudioTranscoder::QnFfmpegAudioTranscoder(AVCodecID codecId):
    QnAudioTranscoder(codecId),
    m_audioEncodingBuffer(0),
    m_encoderCtx(0),
    m_decoderContext(0),
    m_firstEncodedPts(AV_NOPTS_VALUE),
    m_unresampledData(CL_MEDIA_ALIGNMENT, AVCODEC_MAX_AUDIO_FRAME_SIZE),
    m_resampledData(CL_MEDIA_ALIGNMENT, AVCODEC_MAX_AUDIO_FRAME_SIZE),
    m_lastTimestamp(AV_NOPTS_VALUE),
    m_downmixAudio(false),
    m_frameNum(0),
    m_resampleCtx(0),
    m_dstSampleRate(0)
{
    m_bitrate = 128*1000;
    m_audioEncodingBuffer = (quint8*) qMallocAligned(AVCODEC_MAX_AUDIO_FRAME_SIZE, CL_MEDIA_ALIGNMENT);
}

QnFfmpegAudioTranscoder::~QnFfmpegAudioTranscoder()
{
    qFreeAligned(m_audioEncodingBuffer);
    if (m_resampleCtx)
        audio_resample_close(m_resampleCtx);

    QnFfmpegHelper::deleteAvCodecContext(m_encoderCtx);
    QnFfmpegHelper::deleteAvCodecContext(m_decoderContext);
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

bool QnFfmpegAudioTranscoder::open(const QnConstMediaContextPtr& context)
{
    NX_ASSERT(context);

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

    m_encoderCtx->channels = context->getChannels();
    int maxEncoderChannels = getMaxAudioChannels(avCodec);

    if (m_encoderCtx->channels > maxEncoderChannels)
    {
        if (maxEncoderChannels == 2)
            m_downmixAudio = true; //< downmix to stereo inplace before ffmpeg based resample (just not change behaviour, keep as in the previous version)
        m_encoderCtx->channels = maxEncoderChannels;
    }
    m_encoderCtx->sample_rate = context->getSampleRate();
    if (m_dstSampleRate > 0)
        m_encoderCtx->sample_rate = m_dstSampleRate;
    else
        m_encoderCtx->sample_rate = getDefaultDstSampleRate(context->getSampleRate(), avCodec);

    m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    m_encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    m_encoderCtx->bit_rate = 64000 * m_encoderCtx->channels;


    if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize audio encoder.");
        return false;
    }

    avCodec = avcodec_find_decoder(context->getCodecId());
    m_decoderContext = avcodec_alloc_context3(0);
    QnFfmpegHelper::mediaContextToAvCodecContext(m_decoderContext, context);
    if (avcodec_open2(m_decoderContext, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize audio decoder.");
        return false;
    }

    m_context = QnConstMediaContextPtr(new QnAvCodecMediaContext(m_encoderCtx));
    m_frameNum = 0;
    return true;
}

bool QnFfmpegAudioTranscoder::isOpened() const
{
    return m_context != nullptr;
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

int fullSampleSize(AVCodecContext* ctx)
{
    return ctx->channels * sampleSize(ctx->sample_fmt);
}

bool QnFfmpegAudioTranscoder::existMoreData() const
{
    int encoderFrameSize = m_encoderCtx->frame_size * sampleSize(m_encoderCtx->sample_fmt) * m_encoderCtx->channels;
    return m_resampledData.size() >= encoderFrameSize;
}

int QnFfmpegAudioTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    if( result )
        result->reset();

    if (!m_lastErrMessage.isEmpty())
        return -3;

    if (media) {
        if (qAbs(media->timestamp - m_lastTimestamp) > MAX_AUDIO_JITTER || (quint64)m_lastTimestamp == AV_NOPTS_VALUE)
        {
            m_frameNum = 0;
            m_firstEncodedPts = media->timestamp;
        }

        m_lastTimestamp = media->timestamp;
        QnConstCompressedAudioDataPtr audio = std::dynamic_pointer_cast<const QnCompressedAudioData>(media);
        AVPacket avpkt;
        av_init_packet(&avpkt);
        avpkt.data = const_cast<quint8*>((const quint8*)media->data());
        avpkt.size = static_cast<int>(media->dataSize());

        int decodedAudioSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        // TODO: #vasilenko avoid using deprecated methods

        bool needResample=
            m_encoderCtx->channels != m_decoderContext->channels ||
            m_encoderCtx->sample_rate != m_decoderContext->sample_rate ||
            m_encoderCtx->sample_fmt != m_decoderContext->sample_fmt;

        QnByteArray& bufferToDecode = needResample ? m_unresampledData : m_resampledData;
        quint8* decodedDataEndPtr = (quint8*) bufferToDecode.data() + bufferToDecode.size();

        int len = avcodec_decode_audio3(m_decoderContext, (short *)(decodedDataEndPtr), &decodedAudioSize, &avpkt);
        if (m_encoderCtx->frame_size == 0)
            m_encoderCtx->frame_size = m_decoderContext->frame_size;

        if (len < 0)
            return -3;

        if (decodedAudioSize > 0)
        {
            if (m_downmixAudio)
                decodedAudioSize = QnAudioProcessor::downmix(decodedDataEndPtr, decodedAudioSize, m_decoderContext);
            bufferToDecode.resize(bufferToDecode.size() + decodedAudioSize);

            if (needResample)
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
                        16, 10, 0, 0.8);
                }

                int inSamples = bufferToDecode.size() / fullSampleSize(m_decoderContext);
                int outSamlpes = audio_resample(
                    m_resampleCtx,
                    (short*) (m_resampledData.data() + m_resampledData.size()),
                    (short*) m_unresampledData.data(),
                    inSamples);
                int resampledDataBytes = outSamlpes * fullSampleSize(m_encoderCtx);
                m_resampledData.resize(m_resampledData.size() + resampledDataBytes);
                m_unresampledData.clear();
            }
        }
        NX_ASSERT(m_resampledData.size() < AVCODEC_MAX_AUDIO_FRAME_SIZE);
    }

    if( !result )
    {
        m_resampledData.clear(); //< we asked to skip input data
        return 0;
    }

    int encoderFrameSize = m_encoderCtx->frame_size * sampleSize(m_encoderCtx->sample_fmt) * m_encoderCtx->channels;

    // TODO: #vasilenko avoid using deprecated methods
    int encoded = 0;
    while (encoded == 0 && m_resampledData.size() >= encoderFrameSize)
    {
        encoded = avcodec_encode_audio(m_encoderCtx, m_audioEncodingBuffer, FF_MIN_BUFFER_SIZE, (const short*) m_resampledData.data());
        if (encoded < 0)
            return -3; //< TODO: needs refactor. add enum with error codes
        int resampledBufferRest = m_resampledData.size() - encoderFrameSize;
        memmove(m_resampledData.data(), m_resampledData.data() + encoderFrameSize, resampledBufferRest);
        m_resampledData.resize(resampledBufferRest);
    }

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

void QnFfmpegAudioTranscoder::setSampleRate(int value)
{
    m_dstSampleRate = value;
}

#endif // ENABLE_DATA_PROVIDERS
