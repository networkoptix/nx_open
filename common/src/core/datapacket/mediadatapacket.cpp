#include "mediadatapacket.h"

#include "libavformat/avformat.h"
#include "utils/media/ffmpeg_helper.h"

extern QMutex global_ffmpeg_mutex;

QnMediaContext::QnMediaContext(AVCodecContext* ctx)
{
    m_ctx = avcodec_alloc_context();
    avcodec_copy_context(m_ctx, ctx);
}

QnMediaContext::QnMediaContext(CodecID codecId)
{
    if (codecId != CODEC_ID_NONE)
    {
        QMutexLocker mutex(&global_ffmpeg_mutex);
        m_ctx = avcodec_alloc_context();
        AVCodec* codec = avcodec_find_decoder(codecId);
        avcodec_open(m_ctx, codec);
    }
}

QnMediaContext::QnMediaContext(const quint8* payload, int dataSize)

{
    m_ctx = QnFfmpegHelper::deserializeCodecContext((const char*) payload, dataSize);
}

QnMediaContext::~QnMediaContext()
{
    avcodec_close(m_ctx);
    av_free(m_ctx);
}

AVCodecContext* QnMediaContext::ctx() const
{
    return m_ctx;
}

bool QnMediaContext::equalTo(QnMediaContext* other) const
{
    return m_ctx->codec_id == other->m_ctx->codec_id;
}
