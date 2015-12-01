#include "media_context.h"

extern "C"
{
#include <libavformat/avformat.h>
}

#include <utils/media/ffmpeg_helper.h>
#include <utils/media/codec_helper.h>

QnMediaContext::QnMediaContext(const AVCodecContext* ctx)
{
    m_ctx = avcodec_alloc_context3(NULL);
    avcodec_copy_context(m_ctx, ctx);
}

QnMediaContext::QnMediaContext(CodecID codecId)
{
    if (codecId != CODEC_ID_NONE)
    {
        AVCodec* codec = avcodec_find_decoder(codecId);
        if( codec && codec->id == codecId )
        {
            m_ctx = avcodec_alloc_context3(codec);
            m_ctx->codec_id = codecId;
            m_ctx->codec_type = codec->type;
        }
        else
        {
            AVCodec* codec = (AVCodec*)av_malloc(sizeof(AVCodec));
            memset( codec, 0, sizeof(*codec) );
            codec->id = codecId;
            codec->type = AVMEDIA_TYPE_VIDEO;
            m_ctx = avcodec_alloc_context3(codec);
            m_ctx->codec_id = codecId;
        }
    } else {
        m_ctx = 0;
    }
}

QnMediaContext::QnMediaContext(const QByteArray& payload)
{
    m_ctx = QnFfmpegHelper::deserializeCodecContext(payload.data(), payload.size());
}

QnMediaContext::QnMediaContext(const quint8* payload, int dataSize)
{
    m_ctx = QnFfmpegHelper::deserializeCodecContext((const char*) payload, dataSize);
}

QnMediaContext::~QnMediaContext()
{
    QnFfmpegHelper::deleteCodecContext(m_ctx);
    m_ctx = 0;
}

AVCodecContext* QnMediaContext::ctx() const
{
    return m_ctx;
}

QString QnMediaContext::getCodecName() const 
{
    return m_ctx ? codecIDToString(m_ctx->codec_id) : QString();
}

bool QnMediaContext::isSimilarTo(const QnConstMediaContextPtr& other) const
{
    // I've added new condition bits_per_coded_sample for G726 audio codec.
    if( m_ctx == NULL && other->m_ctx == NULL )
        return true;
    if( m_ctx == NULL || other->m_ctx == NULL )
        return false;
    if (m_ctx->width != other->m_ctx->width || m_ctx->height != other->m_ctx->height)
        return false;

    return m_ctx->codec_id == other->m_ctx->codec_id && m_ctx->bits_per_coded_sample == other->m_ctx->bits_per_coded_sample;
}

QnMediaContext* QnMediaContext::cloneWithoutExtradata() const
{
    QnMediaContext* clone = new QnMediaContext(m_ctx);
    clone->m_ctx->extradata_size = 0;
    return clone;
}

QByteArray QnMediaContext::serialize() const
{
    // TODO mike: REWRITE
    QByteArray result;
    if (m_ctx)
        QnFfmpegHelper::serializeCodecContext(m_ctx, &result);
    return result;
}

///////////////////////////////////////////////////////////////////////////

CodecID QnMediaContext::getCodecId() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return CODEC_ID_NONE;

    return m_ctx->codec_id;
}

AVMediaType QnMediaContext::getCodecType() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return AVMEDIA_TYPE_UNKNOWN;

    return m_ctx->codec_type;
}

int QnMediaContext::getExtradataSize() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->extradata_size;
}

uint8_t* QnMediaContext::getExtradata() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->extradata;
}

int QnMediaContext::getSampleRate() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->sample_rate;
}

int QnMediaContext::getChannels() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->channels;
}

AVSampleFormat QnMediaContext::getSampleFmt() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return AV_SAMPLE_FMT_NONE;

    return m_ctx->sample_fmt;
}

int QnMediaContext::getBitRate() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->bit_rate;
}

quint64 QnMediaContext::getChannelLayout() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->channel_layout;
}

int QnMediaContext::getBlockAlign() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->block_align;
}

int QnMediaContext::getBitsPerCodedSample() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->bits_per_coded_sample;
}

PixelFormat QnMediaContext::getPixFmt() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return PIX_FMT_NONE;

    return m_ctx->pix_fmt;
}

int QnMediaContext::getWidth() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->width;
}

int QnMediaContext::getHeight() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->height;
}

int QnMediaContext::getCodedWidth() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->coded_width;
}

int QnMediaContext::getCodedHeight() const
{
    // TODO mike: REWRITE

    if (!m_ctx)
        return 0;

    return m_ctx->coded_height;
}
