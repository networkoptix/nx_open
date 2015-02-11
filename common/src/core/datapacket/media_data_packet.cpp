#include "media_data_packet.h"

#ifdef ENABLE_DATA_PROVIDERS

extern "C"
{
#include <libavformat/avformat.h>
}

#include <QtGui/QRegion>

#include <utils/media/bitStream.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/sse_helper.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>

#include <plugins/camera_plugin.h>

#ifdef Q_OS_MAC
#include <smmintrin.h>
#endif



QnMediaContext::QnMediaContext(AVCodecContext* ctx)
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
            avcodec_open2(m_ctx, codec, NULL);
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
    if (m_ctx) {
        if (m_ctx->codec)
            avcodec_close(m_ctx);
        av_freep(&m_ctx->rc_override);
        av_freep(&m_ctx->intra_matrix);
        av_freep(&m_ctx->inter_matrix);
        av_freep(&m_ctx->extradata);
        av_freep(&m_ctx->rc_eq);
        av_freep(&m_ctx);
    }
}


AVCodecContext* QnMediaContext::ctx() const
{
    return m_ctx;
}

QString QnMediaContext::codecName() const 
{
    return m_ctx ? codecIDToString(m_ctx->codec_id) : QString();
}

bool QnMediaContext::equalTo(QnMediaContext *other) const
{
    // I've add new condition bits_per_coded_sample for G726 audio codec
    if( m_ctx == NULL && other->m_ctx == NULL )
        return true;
    if( m_ctx == NULL || other->m_ctx == NULL )
        return false;
    if (m_ctx->width != other->m_ctx->width || m_ctx->height != other->m_ctx->height)
        return false;
    
    return m_ctx->codec_id == other->m_ctx->codec_id && m_ctx->bits_per_coded_sample == other->m_ctx->bits_per_coded_sample;
}


QnAbstractMediaData::QnAbstractMediaData( DataType _dataType )
: 
    dataType(_dataType),
    compressionType(CODEC_ID_NONE),
    flags(MediaFlags_None),
    channelNumber(0),
    context(0),
    opaque(0)
{
}

QnAbstractMediaData::~QnAbstractMediaData()
{
}

void QnAbstractMediaData::assign(const QnAbstractMediaData* other)
{
    dataProvider = other->dataProvider;
    timestamp = other->timestamp;
    dataType = other->dataType;
    compressionType = other->compressionType;
    flags = other->flags;
    channelNumber = other->channelNumber;
    context = other->context;
    opaque = other->opaque;
}

// ----------------------------------- QnAbstractMediaData -----------------------------------------

//QnAbstractMediaData* QnAbstractMediaData::clone() const
//{
//    QnAbstractMediaData* rez = new QnAbstractMediaData(m_data.getAlignment(), m_data.size());
//    rez->assign(this);
//    return rez;
//}

QnEmptyMediaData* QnEmptyMediaData::clone( QnAbstractAllocator* allocator ) const
{
    QnEmptyMediaData* rez = new QnEmptyMediaData( allocator );
    rez->assign(this);
    rez->m_data.write(m_data.constData(), m_data.size());
    return rez;
}


// ----------------------------------- QnMetaDataV1 -----------------------------------------

QnMetaDataV1::QnMetaDataV1(int initialValue)
:   //TODO #ak delegate constructor
    QnAbstractMediaData(META_V1),
    m_data(CL_MEDIA_ALIGNMENT, MD_WIDTH*MD_HEIGHT/8)
{
    flags = 0;
    m_input = 0;
    m_duration = 0;
    m_firstTimestamp = AV_NOPTS_VALUE;
    timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    if (initialValue)
        m_data.writeFiller(0xff, m_data.capacity());
    else
        m_data.writeFiller(0, m_data.capacity());
}

QnMetaDataV1::QnMetaDataV1(
    QnAbstractAllocator* allocator,
    int initialValue)
:
    QnAbstractMediaData(META_V1),
    m_data(allocator, CL_MEDIA_ALIGNMENT, MD_WIDTH*MD_HEIGHT/8)
{
    flags = 0;
    m_input = 0;
    m_duration = 0;
    m_firstTimestamp = AV_NOPTS_VALUE;
    timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    if (initialValue)
        m_data.writeFiller(0xff, m_data.capacity());
    else
        m_data.writeFiller(0, m_data.capacity());
}

QnMetaDataV1Ptr QnMetaDataV1::fromLightData(const QnMetaDataV1Light& lightData)
{
    QnMetaDataV1Ptr result(new QnMetaDataV1());
    result->timestamp = lightData.startTimeMs*1000;
    result->m_duration = lightData.durationMs*1000;
    result->channelNumber = lightData.channel;
    result->m_input = lightData.input;
    memcpy(result->m_data.data(), lightData.data, result->m_data.size());
    return result;
}

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
inline bool sse4_attribute mathImage_sse41(const __m128i* data, const __m128i* mask, int maskStart, int maskEnd)
{
    for (int i = maskStart; i <= maskEnd; ++i)
    {
        if (_mm_testz_si128(mask[i], data[i]) == 0) /* SSE4. */
            return true;
    }
    return false;
}

inline bool mathImage_sse2(const __m128i* data, const __m128i* mask, int maskStart, int maskEnd)
{
    static const __m128i zerroValue = _mm_setr_epi32(0, 0, 0, 0);
    for (int i = maskStart; i <= maskEnd; ++i)
    {
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_and_si128(mask[i], data[i]), zerroValue)) != 0xffff) /* SSE2. */
            return true;
    }
    return false;
}
#endif    //__i386

inline bool mathImage_cpu(const simd128i* data, const simd128i* mask, int maskStart, int maskEnd)
{
    uint64_t* curPtr = (uint64_t*) data;
    curPtr += maskStart*2;
    uint64_t* maskPtr = (uint64_t*) mask; 
    maskPtr += maskStart*2;

    for (int i = maskStart; i <= maskEnd; ++i)
    {
        if (*curPtr++ & *maskPtr++)
            return true;
        if (*curPtr++ & *maskPtr++)
            return true;
    }
    return false;
}

bool QnMetaDataV1::matchImage(const simd128i* data, const simd128i* mask, int maskStart, int maskEnd)
{
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    if (useSSE41())
        return mathImage_sse41(data, mask, maskStart, maskEnd);
    else 
        return mathImage_sse2(data, mask, maskStart, maskEnd);
#elif __arm__ && __ARM_NEON__
    //TODO/ARM
    return mathImage_cpu(data, mask, maskStart, maskEnd);
#else
    return mathImage_cpu(data, mask, maskStart, maskEnd);
#endif
}


void QnMetaDataV1::assign(const QnMetaDataV1* other)
{
    QnAbstractMediaData::assign(other);
    m_data = other->m_data;
    m_input = other->m_input;
    m_duration = other->m_duration;
    m_firstTimestamp = other->m_firstTimestamp;
}

QnMetaDataV1* QnMetaDataV1::clone( QnAbstractAllocator* allocator ) const
{
    QnMetaDataV1* rez = new QnMetaDataV1( allocator );
    rez->assign(this);
    return rez;
}

void QnMetaDataV1::addMotion(QnConstMetaDataV1Ptr data)
{
    addMotion((const quint8*) data->m_data.data(), data->timestamp);
}

void QnMetaDataV1::addMotion(QnMetaDataV1Ptr data)
{
    addMotion((const quint8*) data->m_data.data(), data->timestamp);
}

void QnMetaDataV1::removeMotion(const simd128i* image, int startIndex, int endIndex)
{
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    __m128i* dst = (__m128i*) m_data.data();
    __m128i* src = (__m128i*) image;
    for (int i = startIndex; i <= endIndex; ++i)
    {
        *dst = _mm_andnot_si128(*src, *dst); /* SSE2. */
        dst++;
        src++;

    }
#else
    // remove without SIMD
    int64_t* dst = (int64_t*) m_data.data();
    int64_t* src = (int64_t*) image;
    for (int i = startIndex; i <= endIndex; ++i)
    {
        *dst++ &= *src++;
        *dst++ &= *src++;
    }
#endif
}

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
inline bool metadataIsEmpty_sse2(__m128i* src)
{
    static const __m128i zerroValue = _mm_setr_epi32(0, 0, 0, 0); /* SSE2. */
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    {
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(src[i], zerroValue)) != 0xffff) /* SSE2. */
            return false;
    }
    return true;
}

inline bool sse4_attribute metadataIsEmpty_sse41(__m128i* src)
{
    static const __m128i ff_mask = _mm_setr_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff); /* SSE2. */
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    {
        if (_mm_testz_si128(src[i], ff_mask) == 0) /* SSE4. */
            return false;
    }
    return true;
}
#endif

inline bool metadataIsEmpty_cpu(const char* data)
{
    const quint32* curPtr = (const quint32*) data;
    for (size_t i = 0; i < MD_WIDTH*MD_HEIGHT/sizeof(quint32)/CHAR_BIT; ++i)
    {
        if (*curPtr++)
            return false;
    }
    return true;
}

bool QnMetaDataV1::isEmpty() const
{
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    if (useSSE41())
        return metadataIsEmpty_sse41((__m128i*) m_data.data());
    else 
        return metadataIsEmpty_sse2((__m128i*) m_data.data());
#elif __arm__ && __ARM_NEON__
    return metadataIsEmpty_cpu(m_data.data());
#else
    return metadataIsEmpty_cpu(m_data.data());
#endif
}

void QnMetaDataV1::assign( const nxcip::Picture& motionPicture, qint64 timestamp, qint64 duration )
{
    if( motionPicture.pixelFormat() != nxcip::PIX_FMT_MONOBLACK )
        return;

    assert( motionPicture.width() == MD_HEIGHT && motionPicture.height() == MD_WIDTH );

    if( motionPicture.xStride(0)*CHAR_BIT == motionPicture.width() )
        memcpy( m_data.data(), motionPicture.data(), MD_WIDTH*MD_HEIGHT/CHAR_BIT );
    else
        assert( false );

    m_firstTimestamp = timestamp;
    m_duration = duration;
}

void QnMetaDataV1::addMotion(const quint8* image, qint64 timestamp)
{
    if ((quint64)m_firstTimestamp == AV_NOPTS_VALUE)
        m_firstTimestamp = timestamp;
    else 
        m_duration = qMax(m_duration, timestamp - m_firstTimestamp);

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    __m128i* dst = (__m128i*) m_data.data();
    __m128i* src = (__m128i*) image;
    for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    {
        *dst = _mm_or_si128(*dst, *src); /* SSE2. */
        dst++;
        src++;

    }
#else
    // remove without SIMD
    memcpy( m_data.data(), image, MD_WIDTH*MD_HEIGHT/CHAR_BIT );
    //int64_t* dst = (int64_t*) m_data.data();
    //int64_t* src = (int64_t*) image;
    //for (int i = 0; i < MD_WIDTH*MD_HEIGHT/128; ++i)
    //{
    //    *dst++ |= *src++;
    //    *dst++ |= *src++;
    //}
#endif
}

bool QnMetaDataV1::isMotionAt(int x, int y, char* mask)
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HEIGHT);

    int shift = x*MD_HEIGHT + y;
    unsigned char b = *((unsigned char*)mask + shift/8 );
    return b & (128 >> (shift&7));
}

bool QnMetaDataV1::isMotionAt(int x, int y) const
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HEIGHT);

    int shift = x*MD_HEIGHT + y;
    unsigned char b = *((unsigned char*)m_data.data() + shift/8 );
    return b & (128 >> (shift&7));
}

void QnMetaDataV1::setMotionAt(int x, int y) 
{
    Q_ASSERT(x<MD_WIDTH);
    Q_ASSERT(y<MD_HEIGHT);

    int shift = x*MD_HEIGHT + y;
    quint8* b = (quint8*)m_data.data() + shift/8;
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
    else if( timestamp <= timeUsec && timestamp + m_duration > timeUsec )
        return true;
    else
        return false;
}

inline void setBit(quint8* data, int x, int y)
{
    bool correctData = (x >= 0 && x < MD_WIDTH) && (y >= 0 && y < MD_HEIGHT);

    Q_ASSERT(correctData);

    if (!correctData)
        return;

    int offset = (x * MD_HEIGHT + y) / 8;
    data[offset] |= 0x80 >> (y&7);
}


void QnMetaDataV1::createMask(const QRegion& region,  char* mask, int* maskStart, int* maskEnd)
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
            {
                setBit((quint8*) mask, x,y);
            }
        }
    }

}

void QnMetaDataV1::serialize(QIODevice* ioDevice) const
{
    Q_ASSERT(channelNumber <= 255);
    qint64 timeStampMs = htonll(timestamp/1000);
    int durationMs = htonl(m_duration/1000);
    ioDevice->write((const char*) &timeStampMs, sizeof(qint64));
    ioDevice->write((const char*) &durationMs, sizeof(int));
    quint8 channel8 = channelNumber;
    ioDevice->write((const char*) &channel8, 1);
    ioDevice->write((const char*) &m_input, 1);
    quint16 reserved  = 0;
    ioDevice->write((const char*) &reserved, 2);
    ioDevice->write(m_data.data(), m_data.size());
}

/*
void QnMetaDataV1::deserialize(QIODevice* ioDevice)
{
    qint64 timeMs = 0;
    int durationMs = 0;

    ioDevice->read((char*) &timeMs, sizeof(timeMs));
    ioDevice->read((char*) &durationMs, 3);
    ioDevice->read((char*) &m_input, 1);
    ioDevice->read(m_data.data(), m_data.size());

    timeMs = ntohll(timeMs);
    durationMs = ntohl(durationMs);
}
*/

bool operator< (const QnMetaDataV1Light& data, const quint64 timeMs)
{
    return data.startTimeMs < timeMs;
}

bool operator< (const quint64 timeMs, const QnMetaDataV1Light& data)
{
    return timeMs < data.startTimeMs;
}

#endif // ENABLE_DATA_PROVIDERS

