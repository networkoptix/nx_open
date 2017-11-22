#include "media_data_packet.h"

#include <QtGui/QRegion>

#include <utils/media/bitStream.h>
#include <utils/common/synctime.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/socket_common.h>
#include <nx/streaming/config.h>

#include <motion/motion_detection.h>

#ifdef Q_OS_OSX
#include <smmintrin.h>
#endif

//----------------------------------- QnAbstractMediaData ----------------------------------------

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(/* global namespace */, MediaQuality,
    (MEDIA_Quality_High, "high")
    (MEDIA_Quality_Low, "low")
    (MEDIA_Quality_ForceHigh, "force-high")
    (MEDIA_Quality_Auto, "auto")
    (MEDIA_Quality_CustomResolution, "custom")
    (MEDIA_Quality_LowIframesOnly, "low-iframes-only")
    (MEDIA_Quality_None, "")
)

bool isLowMediaQuality(MediaQuality q)
{
    return q == MEDIA_Quality_Low || q == MEDIA_Quality_LowIframesOnly;
}

QnAbstractMediaData::QnAbstractMediaData( DataType _dataType ):
    dataType(_dataType),
    compressionType(AV_CODEC_ID_NONE),
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

bool QnAbstractMediaData::isLQ() const
{
    return flags & MediaFlags_LowQuality;
}

bool QnAbstractMediaData::isLive() const
{
    return flags & MediaFlags_LIVE;
}

//------------------------------------- QnEmptyMediaData -----------------------------------------

QnEmptyMediaData::QnEmptyMediaData():
    QnAbstractMediaData(EMPTY_DATA),
    m_data(16,0)
{
}

QnEmptyMediaData::QnEmptyMediaData(QnAbstractAllocator* allocator):
    QnAbstractMediaData(EMPTY_DATA),
    m_data(allocator, 16, 0)
{
}

QnEmptyMediaData* QnEmptyMediaData::clone( QnAbstractAllocator* allocator ) const
{
    QnEmptyMediaData* rez = new QnEmptyMediaData( allocator );
    rez->assign(this);
    rez->m_data.write(m_data.constData(), m_data.size());
    return rez;
}

const char*QnEmptyMediaData::data() const
{
    return m_data.data();
}

size_t QnEmptyMediaData::dataSize() const
{
    return m_data.size();
}

//--------------------------------------- QnMetaDataV1 -------------------------------------------

//TODO #ak delegate constructor
QnMetaDataV1::QnMetaDataV1(int initialValue):
    QnAbstractCompressedMetadata(MetadataType::Motion)
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

QnMetaDataV1::QnMetaDataV1(QnAbstractAllocator* allocator, int initialValue):
    QnAbstractCompressedMetadata(MetadataType::Motion, allocator)
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
        if (_mm_testz_si128_sse4(mask[i], data[i]) == 0) /* SSE4. */
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
#endif //__i386

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

const char* QnMetaDataV1::data() const
{
    return m_data.data();
}

char* QnMetaDataV1::data()
{
    return m_data.data();
}

size_t QnMetaDataV1::dataSize() const
{
    return m_data.size();
}

void QnMetaDataV1::addMotion(QnConstMetaDataV1Ptr data)
{
    addMotion((const quint8*) data->m_data.data(), data->timestamp);
}

void QnMetaDataV1::addMotion(QnMetaDataV1Ptr data)
{
    addMotion((const quint8*) data->m_data.data(), data->timestamp);
}

void QnMetaDataV1::removeMotion(const simd128i* image)
{
    const int endIndex = Qn::kMotionGridWidth*Qn::kMotionGridHeight / 128 - 1;
#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    __m128i* dst = (__m128i*) m_data.data();
    __m128i* src = (__m128i*) image;
    for (int i = 0; i <= endIndex; ++i)
    {
        *dst = _mm_andnot_si128(*src, *dst); /* SSE2. */
        dst++;
        src++;
    }
#else
    // remove without SIMD
    int64_t* dst = (int64_t*) m_data.data();
    int64_t* src = (int64_t*) image;
    for (int i = 0; i <= endIndex; ++i)
    {
        *dst &= ~(*src);
        ++dst;
        ++src;
        *dst &= ~(*src);
        ++dst;
        ++src;
    }
#endif
}

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
inline bool metadataIsEmpty_sse2(__m128i* src)
{
    static const __m128i zerroValue = _mm_setr_epi32(0, 0, 0, 0); /* SSE2. */
    for (int i = 0; i < Qn::kMotionGridWidth*Qn::kMotionGridHeight/128; ++i)
    {
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(src[i], zerroValue)) != 0xffff) /* SSE2. */
            return false;
    }
    return true;
}

inline bool sse4_attribute metadataIsEmpty_sse41(__m128i* src)
{
    static const __m128i ff_mask = _mm_setr_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff); /* SSE2. */
    for (int i = 0; i < Qn::kMotionGridWidth*Qn::kMotionGridHeight/128; ++i)
    {
        if (_mm_testz_si128_sse4(src[i], ff_mask) == 0) /* SSE4. */
            return false;
    }
    return true;
}
#endif

inline bool metadataIsEmpty_cpu(const char* data)
{
    const quint32* curPtr = (const quint32*) data;
    for (size_t i = 0; i < Qn::kMotionGridWidth*Qn::kMotionGridHeight/sizeof(quint32)/CHAR_BIT; ++i)
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

void QnMetaDataV1::assign( const void* data, qint64 timestamp, qint64 duration )
{
    NX_ASSERT( m_data.size() == Qn::kMotionGridWidth * Qn::kMotionGridHeight / CHAR_BIT );

    memcpy( m_data.data(), data, Qn::kMotionGridWidth * Qn::kMotionGridHeight / CHAR_BIT );

    m_firstTimestamp = timestamp;
    m_duration = duration;
}

void QnMetaDataV1::addMotion(const quint8* image, qint64 timestamp)
{
    if (m_firstTimestamp == AV_NOPTS_VALUE)
        m_firstTimestamp = timestamp;
    else
        m_duration = qMax(m_duration, timestamp - m_firstTimestamp);

#if defined(__i386) || defined(__amd64) || defined(_WIN32)
    __m128i* dst = (__m128i*) m_data.data();
    __m128i* src = (__m128i*) image;
    for (int i = 0; i < Qn::kMotionGridWidth*Qn::kMotionGridHeight/128; ++i)
    {
        *dst = _mm_or_si128(*dst, *src); /* SSE2. */
        dst++;
        src++;

    }
#else
    // remove without SIMD
    int64_t* dst = (int64_t*) m_data.data();
    int64_t* src = (int64_t*) image;
    for (int i = 0; i < Qn::kMotionGridWidth*Qn::kMotionGridHeight/128; ++i)
    {
        *dst++ |= *src++;
        *dst++ |= *src++;
    }
#endif
}

bool QnMetaDataV1::isMotionAt(int x, int y, char* mask)
{
    NX_ASSERT(x<Qn::kMotionGridWidth);
    NX_ASSERT(y<Qn::kMotionGridHeight);

    int shift = x*Qn::kMotionGridHeight + y;
    unsigned char b = *((unsigned char*)mask + shift/8 );
    return b & (128 >> (shift&7));
}

bool QnMetaDataV1::isMotionAt(int x, int y) const
{
    NX_ASSERT(x<Qn::kMotionGridWidth);
    NX_ASSERT(y<Qn::kMotionGridHeight);

    int shift = x*Qn::kMotionGridHeight + y;
    unsigned char b = *((unsigned char*)m_data.data() + shift/8 );
    return b & (128 >> (shift&7));
}

void QnMetaDataV1::setMotionAt(int x, int y)
{
    NX_ASSERT(x<Qn::kMotionGridWidth);
    NX_ASSERT(y<Qn::kMotionGridHeight);

    int shift = x*Qn::kMotionGridHeight + y;
    quint8* b = (quint8*)m_data.data() + shift/8;
    *b |= (128 >> (shift&7));
}

bool QnMetaDataV1::mapMotion(const QRect& imageRect, const QRect& mRect)
{
    QRect motioRect = imageRect.intersected(mRect);

    if (motioRect.isNull())
        return false;

    int leftZone = motioRect.left() * Qn::kMotionGridWidth / imageRect.width();
    int topZone = motioRect.top() * Qn::kMotionGridHeight / imageRect.height();

    int rightZone = motioRect.right() * Qn::kMotionGridWidth / imageRect.width();
    int bottomZone = motioRect.bottom() * Qn::kMotionGridHeight / imageRect.height();

    for (int x = leftZone; x <= rightZone; ++x) {
        for (int y = topZone; y <= bottomZone; ++y)
        {
            setMotionAt(x, y);
        }
    }
    return true;
}

bool QnMetaDataV1::isInput(int index) const
{
    return (m_input >> index) & 1;
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
    bool correctData = (x >= 0 && x < Qn::kMotionGridWidth) && (y >= 0 && y < Qn::kMotionGridHeight);

    NX_ASSERT(correctData);

    if (!correctData)
        return;

    int offset = (x * Qn::kMotionGridHeight + y) / 8;
    data[offset] |= 0x80 >> (y&7);
}

void QnMetaDataV1::createMask(const QRegion& region,  char* mask, int* maskStart, int* maskEnd)
{
    if (maskStart)
        *maskStart = 0;
    if (maskEnd)
        *maskEnd = 0;
    memset(mask, 0, Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8);

    for (int i = 0; i < region.rectCount(); ++i)
    {
        const QRect& rect = region.rects().at(i);
        if (maskStart)
            *maskStart = qMin((rect.left() * Qn::kMotionGridHeight + rect.top()) / 128, *maskStart);
        if (maskEnd)
            *maskEnd = qMax((rect.right() * Qn::kMotionGridHeight + rect.bottom()) / 128, *maskEnd);
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
    NX_ASSERT(channelNumber <= 255);
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

bool operator< (const QnMetaDataV1Light& data, const quint64 timeMs)
{
    return data.startTimeMs < timeMs;
}

bool operator< (const quint64 timeMs, const QnMetaDataV1Light& data)
{
    return timeMs < data.startTimeMs;
}

//--------------------------------- QnAbstractCompressedMetadata ---------------------------------

QnAbstractCompressedMetadata::QnAbstractCompressedMetadata(MetadataType type):
    QnAbstractMediaData(type == MetadataType::Motion ? META_V1 : GENERIC_METADATA),
    metadataType(type),
    m_data(
        CL_MEDIA_ALIGNMENT,
        type == MetadataType::Motion
            ? Qn::kMotionGridWidth*Qn::kMotionGridHeight/8
            : 0)
{
}

QnAbstractCompressedMetadata::QnAbstractCompressedMetadata(MetadataType type, QnAbstractAllocator* allocator):
    QnAbstractMediaData(type == MetadataType::Motion ? META_V1 : GENERIC_METADATA),
    metadataType(type),
    m_data(
        allocator,
        CL_MEDIA_ALIGNMENT,
        type == MetadataType::Motion
            ? Qn::kMotionGridWidth*Qn::kMotionGridHeight/8
            : 0)
{
}

bool QnAbstractCompressedMetadata::containTime(const qint64 timeUsec) const
{
    if (m_duration == 0)
        return timestamp == timeUsec;
    else if( timestamp <= timeUsec && timestamp + m_duration > timeUsec )
        return true;

    return false;
}

//------------------------------------ QnCompressedMetadata --------------------------------------

QnCompressedMetadata::QnCompressedMetadata(MetadataType type):
    QnAbstractCompressedMetadata(type)
{
}

QnCompressedMetadata::QnCompressedMetadata(MetadataType type, QnAbstractAllocator* allocator):
    QnAbstractCompressedMetadata(type, allocator)
{
}

QnAbstractMediaData* QnCompressedMetadata::clone(QnAbstractAllocator* allocator) const
{
    QnCompressedMetadata* cloned = new QnCompressedMetadata(metadataType, allocator);
    cloned->assign(this);
    cloned->m_data.write(m_data.constData(), m_data.size());
    return cloned;
}

const char* QnCompressedMetadata::data() const
{
    return m_data.constData();
}

size_t QnCompressedMetadata::dataSize() const
{
    return m_data.size();
}

bool QnCompressedMetadata::setData(const char* data, std::size_t dataSize)
{
    return m_data.write(data, dataSize) != 0;
}

QnCompressedMetadataPtr QnCompressedMetadata::createMediaEventPacket(
    qint64 timestampUs,
    Qn::MediaStreamEvent value)
{
    QnCompressedMetadataPtr rez(new QnCompressedMetadata(MetadataType::MediaStreamEvent));
    rez->timestamp = timestampUs;
    auto data = QnLexical::serialized(value).toUtf8();
    rez->setData(data.data(), data.size());
    return rez;
}
