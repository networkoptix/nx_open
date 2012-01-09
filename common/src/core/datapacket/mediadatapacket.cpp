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
    } else {
        m_ctx = 0;
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
        if (m_ctx->codec)
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
    QnAbstractMediaData(CL_MEDIA_ALIGNMENT, MD_WIDTH*MD_HEIGHT/8)
{
    dataType = META_V1;
    //useTwice = false;

    flags = 0;
    i_mask = 0x01;
    m_input = 0;
    m_duration = 0;
    m_firstTimestamp = AV_NOPTS_VALUE;
    timestamp = QDateTime::currentMSecsSinceEpoch()*1000;
    data.fill(0, data.capacity());
}

void QnMetaDataV1::addMotion(QnMetaDataV1Ptr data)
{
    addMotion((const quint8*) data->data.data(), data->timestamp);
}

void QnMetaDataV1::removeMotion(const __m128i* image, int startIndex, int endIndex)
{
    __m128i* dst = (__m128i*) data.data();
    __m128i* src = (__m128i*) image;
    for (int i = startIndex; i <= endIndex; ++i)
    {
        *dst = _mm_andnot_si128(*src, *dst);
        dst++;
        src++;

    }
}

bool QnMetaDataV1::isEmpty() const
{
    static const __m128i ff_mask = _mm_setr_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
    static const __m128i zerroValue = _mm_setr_epi32(0, 0, 0, 0);
    __m128i* src = (__m128i*) data.data();

    if (useSSE41())
    {
        for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
        {
            if (_mm_testz_si128(src[i], ff_mask) == 0)
                return false;
        }
    }
    else 
    {
        for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
        {
            if (_mm_movemask_epi8(_mm_cmpeq_epi32(src[i], zerroValue)) == 0)
                return false;
        }
    }
    return true;
}

void QnMetaDataV1::addMotion(const quint8* image, qint64 timestamp)
{
    if (m_firstTimestamp == AV_NOPTS_VALUE)
        m_firstTimestamp = timestamp;
    else 
        m_duration = qMax(m_duration, timestamp - m_firstTimestamp);

    __m128i* dst = (__m128i*) data.data();
    __m128i* src = (__m128i*) image;
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    {
        *dst = _mm_or_si128(*dst, *src);
        dst++;
        src++;

    }
}

bool QnMetaDataV1::isMotionAt(int x, int y) const
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HEIGHT);

    int shift = x*MD_HEIGHT + y;
    unsigned char b = *((unsigned char*)data.data() + shift/8 );
    return b & (128 >> (shift&7));
}

void QnMetaDataV1::setMotionAt(int x, int y) 
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HEIGHT);

    int shift = x*MD_HEIGHT + y;
    quint8* b = (quint8*)data.data() + shift/8;
    *b |= (128 >> (shift&7));
}


bool QnMetaDataV1::mapMotion(const QRect& imageRect, const QRect& mRect)
{
    QRect motioRect = imageRect.intersected(mRect);

    if (motioRect.isNull())
        return false;

    //int localZoneWidth = imageRect.width() / MD_WIDTH;
    //int localZoneHight = imageRect.height() / MD_HEIGHT;

    int leftZone = motioRect.left() * MD_WIDTH / imageRect.width();
    int topZone = motioRect.top() * MD_HEIGHT / imageRect.height();

    int rightZone = motioRect.right() * MD_WIDTH / imageRect.width();
    int bottomZone = motioRect.bottom() * MD_HEIGHT / imageRect.height();

    for (int x = leftZone; x <= rightZone; ++x) {
        for (int y = topZone; y <= bottomZone; ++y)
        {
            setMotionAt(x, y);
        }
    }
    return true;
}

bool QnMetaDataV1::containTime(const qint64 timeUsec) const
{
    if (m_duration == 0)
        return timestamp == timeUsec;
    else
        return timestamp <= timeUsec && timestamp + m_duration > timeUsec;
}

inline void setBit(quint8* data, int x, int y)
{
    int offset = (x * MD_HEIGHT + y) / 8;
    data[offset] |= 0x80 >> (y&7);
}


void QnMetaDataV1::createMask(const QRegion& region,  __m128i* mask, int* maskStart, int* maskEnd)
{
    if (maskStart)
        *maskStart = 0;
    if (maskEnd)
        *maskEnd = 0;
    memset(mask, 0, MD_WIDTH * MD_HEIGHT / 8);

    for (int i = 0; i < region.rectCount(); ++i)
    {
        const QRect& rect = region.rects().at(i);
        if (maskStart)
            *maskStart = qMin((rect.left() * MD_HEIGHT + rect.top()) / 128, *maskStart);
        if (maskEnd)
            *maskEnd = qMax((rect.right() * MD_HEIGHT + rect.bottom()) / 128, *maskEnd);
        for (int x = rect.left(); x <= rect.right(); ++x)
        {
            for (int y = rect.top(); y <= rect.bottom(); ++y)
                setBit((quint8*) mask, x,y);
        }
    }

}

