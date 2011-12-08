#include "mediadatapacket.h"

#include "libavformat/avformat.h"
#include "utils/media/ffmpeg_helper.h"
#include "utils/media/sse_helper.h"

extern QMutex global_ffmpeg_mutex;

QnMediaContext::QnMediaContext(AVCodecContext* ctx)
{
    QMutexLocker mutex(&global_ffmpeg_mutex);
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
    QMutexLocker mutex(&global_ffmpeg_mutex);
    if (m_ctx) {
        avcodec_close(m_ctx);
        av_free(m_ctx);
    }
}

AVCodecContext* QnMediaContext::ctx() const
{
    return m_ctx;
}

bool QnMediaContext::equalTo(QnMediaContext* other) const
{
    return m_ctx->codec_id == other->m_ctx->codec_id;
}

// ----------------------------------- QnMetaDataV1 -----------------------------------------

void QnMetaDataV1::addMotion(const quint8* image, qint64 timestamp)
{
    if (m_firstTimestamp == AV_NOPTS_VALUE)
        m_firstTimestamp = timestamp;
    else 
        m_duration = qMax(m_duration, timestamp - m_firstTimestamp);

    __m128i* dst = (__m128i*) data.data();
    __m128i* src = (__m128i*) image;
    for (int i = 0; i < MD_WIDTH*MD_HIGHT/128; ++i)
    {
        _mm_or_si128(*dst++, *src++);

    }
}
