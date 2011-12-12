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

QnMetaDataV1::QnMetaDataV1():
    QnAbstractMediaData(CL_MEDIA_ALIGNMENT, MD_WIDTH*MD_HIGHT/8)
{
    dataType = META_V1;
    //useTwice = false;

    flags = 0;
    i_mask = 0x01;
    m_input = 0;
    m_duration = 0;
    m_firstTimestamp = AV_NOPTS_VALUE;
    timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch()*1000;
    memset(data.data(), 0, data.capacity());
}

void QnMetaDataV1::addMotion(QnMetaDataV1Ptr data)
{
    addMotion((const quint8*) data->data.data(), data->timestamp);
}

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

bool QnMetaDataV1::isMotionAt(int x, int y) const
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HIGHT);

    int shift = x*MD_HIGHT + y;
    unsigned char b = *((unsigned char*)data.data() + shift/8 );
    return b & (128 >> (shift&7));
}

void QnMetaDataV1::setMotionAt(int x, int y) 
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HIGHT);

    int shift = x*MD_HIGHT + y;
    quint8* b = (quint8*)data.data() + shift/8;
    *b |= (128 >> (shift&7));
}
