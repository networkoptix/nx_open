// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "meta_data_packet.h"

#include <QtCore/QBuffer>
#include <QtCore/QtEndian>
#include <QtGui/QRegion>

#include <nx/utils/log/assert.h>

static const QRect kMaxGridRect(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);

//--------------------------------------- QnMetaDataV1 -------------------------------------------

QnMetaDataV1::QnMetaDataV1()
    : QnAbstractCompressedMetadata(MetadataType::Motion, kMotionDataBufferSize)
{
    m_data.writeFiller(0, m_data.capacity());
}

//TODO #akolesnikov delegate constructor
QnMetaDataV1::QnMetaDataV1(std::chrono::microseconds timestamp_, int initialValue, int extraBufferSize)
    :
    QnAbstractCompressedMetadata(MetadataType::Motion, kMotionDataBufferSize + extraBufferSize)
{
    timestamp = timestamp_.count();
    if (initialValue)
        m_data.writeFiller(0xff, m_data.capacity());
    else
        m_data.writeFiller(0, m_data.capacity());
}

QnMetaDataV1::QnMetaDataV1(std::chrono::microseconds timestamp_, const char* buffer, int extraBufferSize)
    :
    QnAbstractCompressedMetadata(MetadataType::Motion, kMotionDataBufferSize + extraBufferSize)
{
    timestamp = timestamp_.count();
    m_data.write(buffer, kMotionDataBufferSize);
    m_data.writeFiller(0, extraBufferSize);
}

QnMetaDataV1Ptr QnMetaDataV1::fromLightData(const QnMetaDataV1Light& lightData)
{
    QnMetaDataV1Ptr result(new QnMetaDataV1());
    result->timestamp = lightData.startTimeMs*1000;
    result->m_duration = lightData.durationMs*1000;
    result->channelNumber = lightData.channel;
    memcpy(result->m_data.data(), lightData.data, result->m_data.size());
    return result;
}

#if defined(NX_SSE2_SUPPORTED)
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

inline bool mathImage_cpu(const quint64* data, const simd128i* mask, int maskStart, int maskEnd)
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

bool QnMetaDataV1::matchImage(const quint64* data, const simd128i* mask, int maskStart, int maskEnd)
{
    if (uintptr_t (data) % 16)
        return mathImage_cpu(data, mask, maskStart, maskEnd);

    #if defined(NX_SSE2_SUPPORTED)
        if (useSSE41())
            return mathImage_sse41((const simd128i*) data, mask, maskStart, maskEnd);
        else
            return mathImage_sse2((const simd128i*) data, mask, maskStart, maskEnd);
    #elif defined(__arm__) && defined(__ARM_NEON)
        // TODO: ARM.
        return mathImage_cpu(data, mask, maskStart, maskEnd);
    #else
        return mathImage_cpu(data, mask, maskStart, maskEnd);
    #endif
}

void QnMetaDataV1::assign(const QnMetaDataV1* other)
{
    QnAbstractMediaData::assign(other);
    m_data = other->m_data;
    m_duration = other->m_duration;
    m_firstTimestamp = other->m_firstTimestamp;
}

QnMetaDataV1* QnMetaDataV1::clone() const
{
    QnMetaDataV1* rez = new QnMetaDataV1(std::chrono::microseconds(this->timestamp));
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
#if defined(NX_SSE2_SUPPORTED)
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

#if defined(NX_SSE2_SUPPORTED)
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

void QnMetaDataV1Light::doMarshalling()
{
    startTimeMs = qFromBigEndian(startTimeMs);
    durationMs = qFromBigEndian(durationMs);
}

bool QnMetaDataV1::isEmpty() const
{
    #if defined(NX_SSE2_SUPPORTED)
        if (useSSE41())
            return metadataIsEmpty_sse41((__m128i*) m_data.data());
        else
            return metadataIsEmpty_sse2((__m128i*) m_data.data());
    #elif defined(__arm__) && defined(__ARM_NEON)
        // TODO: ARM.
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

    addMotion((quint64*) m_data.data(), (quint64*) image);
}

void QnMetaDataV1::addMotion(quint64* dst64, const quint64* src64)
{
#if defined(NX_SSE2_SUPPORTED)

    if ((uintptr_t (dst64) % 16 == 0) && (uintptr_t (src64) % 16 == 0))
    {
        __m128i* dst = (__m128i*) dst64;
        __m128i* src = (__m128i*) src64;
        for (int i = 0; i < Qn::kMotionGridWidth*Qn::kMotionGridHeight / 128; ++i)
        {
            *dst = _mm_or_si128(*dst, *src); /* SSE2. */
            dst++;
            src++;

        }
        return;
    }
#endif
    // add without SIMD
    for (int i = 0; i < Qn::kMotionGridWidth*Qn::kMotionGridHeight / 128; ++i)
    {
        *dst64++ |= *src64++;
        *dst64++ |= *src64++;
    }
}

void QnMetaDataV1::addMotion(char* dst, const char* src)
{
    return addMotion((quint64*) dst, (const quint64*) src);
}

QRect QnMetaDataV1::rectFromNormalizedRect(const QRectF& rectF)
{
    const qreal kEpsilon = 0.01;

    const qreal x1 = rectF.left() * Qn::kMotionGridWidth;
    const qreal y1 = rectF.top() * Qn::kMotionGridHeight;
    const qreal x2 = rectF.right() * Qn::kMotionGridWidth;
    const qreal y2 = rectF.bottom() * Qn::kMotionGridHeight;

    const int x = x1 + kEpsilon;
    const int y = y1 + kEpsilon;

    auto result = QRect(x, y, x2 - x + (1.0 - kEpsilon), y2 - y + (1.0 - kEpsilon));

    // Making sure the result box is always non-empty and within the grid.
    if (result.left() >= kMaxGridRect.width())
        result.setLeft(kMaxGridRect.width() - 1);
    if (result.top() >= kMaxGridRect.height())
        result.setTop(kMaxGridRect.height() - 1);
    if (result.width() <= 0)
        result.setWidth(1);
    if (result.height() <= 0)
        result.setHeight(1);

    return result.intersected(kMaxGridRect);
}

void QnMetaDataV1::addMotion(const QRectF& rectF)
{
    return addMotion(rectFromNormalizedRect(rectF));
}

void QnMetaDataV1::addMotion(char* buffer, const QRectF& rectF)
{
    addMotion(buffer, rectFromNormalizedRect(rectF));
}

void QnMetaDataV1::addMotion(const QRect& rect)
{
    addMotion(m_data.data(), rect);
}

void QnMetaDataV1::addMotion(char* buffer, const QRect& rect)
{
    const quint32 maskL = (quint32)(-1) >> rect.top();
    const quint32 maskR = (quint32)(-1) << (31 - rect.bottom());
    const quint32 mask = qToBigEndian(maskL & maskR);
    const quint64 mask64 = ((quint64)mask << 32) + mask;

    quint32* data = (quint32*)buffer + rect.left();
    const quint32* const dataEnd = data + rect.width();
    const quint64* const dataEnd64 = (quint64*)(std::uintptr_t(dataEnd) & ~std::uintptr_t(7));
    if (std::uintptr_t(data) % 8 != 0)
        *data++ |= mask;
    quint64* data64 = (quint64*)data;
    NX_ASSERT((std::uintptr_t(data64) & 7) == 0);
    while (data64 < dataEnd64)
        *data64++ |= mask64;
    data = (quint32*)data64;
    if (data < dataEnd)
        *data |= mask;
}

QRect QnMetaDataV1::boundingBox(const char* data)
{
    int left = Qn::kMotionGridWidth;
    int right = 0;
    int top = Qn::kMotionGridHeight;
    int bottom = 0;
    bool hasData = false;
    for (int y = 0; y < Qn::kMotionGridHeight; ++y)
    {
        for (int x = 0; x < Qn::kMotionGridWidth; ++x)
        {
            if (isMotionAt(x, y, data))
            {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
                hasData = true;
            }
        }
    }
    if (!hasData)
        return QRect();
    return QRect(left, top, right - left + 1, bottom - top + 1);
}

bool QnMetaDataV1::isMotionAt(int x, int y, const char* mask)
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

void addDataToMask(const QRect& rect, char* mask, int* maskStart, int* maskEnd)
{
    if (maskStart)
        *maskStart = qMin((rect.left() * Qn::kMotionGridHeight + rect.top()) / 128, *maskStart);
    if (maskEnd)
        *maskEnd = qMax((rect.right() * Qn::kMotionGridHeight + rect.bottom()) / 128, *maskEnd);
    for (int x = rect.left(); x <= rect.right(); ++x)
    {
        for (int y = rect.top(); y <= rect.bottom(); ++y)
        {
            setBit((quint8*)mask, x, y);
        }
    }
}

void QnMetaDataV1::createMask(const QRegion& region,  char* mask, int* maskStart, int* maskEnd)
{
    if (maskStart)
        *maskStart = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 128 - 1;
    if (maskEnd)
        *maskEnd = 0;
    memset(mask, 0, Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8);

    for (const QRect& rect: region)
        addDataToMask(rect.intersected(kMaxGridRect), mask, maskStart, maskEnd);
}

void QnMetaDataV1::createMask(const QRectF& rectF, char* mask, int* maskStart, int* maskEnd)
{
    return createMask(rectFromNormalizedRect(rectF), mask, maskStart, maskEnd);
}

void QnMetaDataV1::createMask(const QRect& data, char* mask, int* maskStart, int* maskEnd)
{
    if (maskStart)
        *maskStart = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 128 - 1;
    if (maskEnd)
        *maskEnd = 0;
    memset(mask, 0, Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8);

    QRect rect = data.intersected(kMaxGridRect);
    addDataToMask(rect, mask, maskStart, maskEnd);
}

void QnMetaDataV1::serialize(QIODevice* ioDevice) const
{
    ioDevice->write(serialize());
}

QByteArray QnMetaDataV1::serialize() const
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    NX_ASSERT(channelNumber <= 255);
    qint64 timeStampMs = qToBigEndian(timestamp / 1000);
    int durationMs = qToBigEndian(int(m_duration / 1000));
    buffer.write((const char*)&timeStampMs, sizeof(qint64));
    buffer.write((const char*)&durationMs, sizeof(int));
    quint8 channel8 = channelNumber;
    buffer.write((const char*)&channel8, 1);
    const quint8 reserved[3] = { 0, 0, 0 };
    buffer.write((const char*)&reserved, 3);
    buffer.write(m_data.data(), m_data.size());
    return buffer.data();
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

QnAbstractCompressedMetadata::QnAbstractCompressedMetadata(MetadataType type, size_t bufferSize):
    QnAbstractMediaData(GENERIC_METADATA),
    metadataType(type),
    m_duration(0),
    m_data(CL_MEDIA_ALIGNMENT, bufferSize, AV_INPUT_BUFFER_PADDING_SIZE)
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

qint64 QnAbstractCompressedMetadata::duration() const
{
    return m_duration;
}

void QnAbstractCompressedMetadata::setData(nx::utils::ByteArray&& buffer)
{
    m_data = std::move(buffer);
}

//------------------------------------ QnCompressedMetadata --------------------------------------

QnCompressedMetadata::QnCompressedMetadata(MetadataType type, int bufferSize):
    QnAbstractCompressedMetadata(type, bufferSize)
{
}

QnAbstractMediaData* QnCompressedMetadata::clone() const
{
    auto cloned = new QnCompressedMetadata(metadataType, 0);
    cloned->assign(this);
    return cloned;
}

void QnCompressedMetadata::assign(const QnAbstractCompressedMetadata* other)
{
    base_type::assign(other);
    m_data = other->m_data;
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

bool QnCompressedMetadata::setData(const QByteArray& data)
{
    return m_data.write(data.data(), data.size());
}

QnCompressedMetadataPtr QnCompressedMetadata::createMediaEventPacket(
    qint64 timestampUs,
    nx::media::StreamEvent value,
    const QByteArray& extraData)
{
    QnCompressedMetadataPtr rez(new QnCompressedMetadata(MetadataType::MediaStreamEvent));
    rez->timestamp = timestampUs;

    nx::media::StreamEventPacket packet;
    packet.code = value;
    packet.extraData = extraData;
    rez->setData(nx::media::serialize(packet));
    return rez;
}

nx::media::StreamEventPacket QnCompressedMetadata::toMediaEventPacket() const
{
    const QByteArray data = QByteArray::fromRawData(m_data.data(), (int) m_data.size());
    nx::media::StreamEventPacket result;
    nx::media::deserialize(data, &result);
    return result;
}
