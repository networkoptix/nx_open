// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>
#include <QtCore/QVector>

#include <nx/media/aligned_allocator.h>
#include <nx/media/media_data_packet.h>
#include <nx/media/motion_detection.h>
#include <nx/media/sse_helper.h>
#include <nx/media/stream_event.h>

struct QnAbstractCompressedMetadata;
struct QnCompressedMetadata;
struct QnMetaDataV1;
struct QnMetaDataV1Light;

using QnMetaDataV1Ptr = std::shared_ptr<QnMetaDataV1>;
using QnConstMetaDataV1Ptr = std::shared_ptr<const QnMetaDataV1>;
using QnMetaDataLightVector = std::vector<QnMetaDataV1Light, QnAlignedAllocator<QnMetaDataV1Light>>;
using QnAbstractCompressedMetadataPtr = std::shared_ptr<QnAbstractCompressedMetadata>;
using QnConstAbstractCompressedMetadataPtr = std::shared_ptr<const QnAbstractCompressedMetadata>;
using QnCompressedMetadataPtr = std::shared_ptr<QnCompressedMetadata>;
using QnConstCompressedMetadataPtr = std::shared_ptr<const QnCompressedMetadata>;

enum class MetadataType
{
    Motion,
    ObjectDetection,
    MediaStreamEvent,
    InStream
};

// TODO: #dmishin get rid of implementations and members in this class
// It should be an interface not a base implementation.
struct NX_MEDIA_CORE_API QnAbstractCompressedMetadata: public QnAbstractMediaData
{

public:
    QnAbstractCompressedMetadata(MetadataType type, size_t bufferSize);

    virtual bool containTime(const qint64 timeUsec) const;

    qint64 duration() const;

    virtual void setData(nx::utils::ByteArray&& buffer) override;

public:
    MetadataType metadataType;
    qint64 m_duration = 0;
    nx::utils::ByteArray m_data;
};

struct NX_MEDIA_CORE_API QnCompressedMetadata: public QnAbstractCompressedMetadata
{
    using base_type = QnAbstractCompressedMetadata;

    QnCompressedMetadata(MetadataType type, int bufferSize = 0);

    virtual QnAbstractMediaData* clone() const override;

    virtual const char* data() const override;
    virtual size_t dataSize() const override;

    bool setData(const char* data, std::size_t dataSize);
    bool setData(const QByteArray& data);
    using QnAbstractCompressedMetadata::setData;

    void setDurationUsec(qint64 value) { m_duration = value;  }
    void setTimestampUsec(qint64 value) { timestamp = value; }

    static QnCompressedMetadataPtr createMediaEventPacket(
        qint64 timestampUs,
        nx::media::StreamEvent value,
        const QByteArray& extraData = QByteArray());

    nx::media::StreamEventPacket toMediaEventPacket() const;

protected:
    void assign(const QnAbstractCompressedMetadata* other);
};

/**
* This structure used for serialized QnMetaDataV1
* Timestamp and duration specified in milliseconds
* structure can be directly mapped to deserialized memory buffer to represent MetaData
*/
#pragma pack(push, 1)
struct NX_MEDIA_CORE_API QnMetaDataV1Light
{

public:
    /**
    * Structure MUST be prepared before use by calling doMarshalling method
    */
    void doMarshalling();

    qint64 endTimeMs() const  { return startTimeMs + durationMs; }

public:
    quint64 startTimeMs;
    quint32 durationMs;
    quint8 channel;
    quint8 reserved0;
    quint16 reserved;
    quint8 data[Qn::kMotionGridWidth * Qn::kMotionGridHeight/8];
};
#pragma pack(pop)

NX_MEDIA_CORE_API bool operator< (const QnMetaDataV1Light& data, const quint64 timeMs);
NX_MEDIA_CORE_API bool operator< (const quint64 timeMs, const QnMetaDataV1Light& data);

struct NX_MEDIA_CORE_API QnMetaDataV1: public QnAbstractCompressedMetadata
{
public:
    static const int kMotionDataBufferSize = Qn::kMotionGridWidth*Qn::kMotionGridHeight / 8;

    QnMetaDataV1();
    QnMetaDataV1(std::chrono::microseconds timestamp, int initialValue = 0, int extraBufferSize = 0);
    QnMetaDataV1(std::chrono::microseconds timestamp_, const char* buffer, int extraBufferSize);

    static QnMetaDataV1Ptr fromLightData(const QnMetaDataV1Light& lightData);

    /**
    * Merge existing motion image with new motion image. Matrix is allowed col to col
    * 0   1
    * |   |
    * \/  \/
    * |   |
    * \/  \/
    * |   |
    * \/  \/
    */
    void addMotion(const quint8* data, qint64 timestamp);
    void addMotion(QnMetaDataV1Ptr data);
    void addMotion(QnConstMetaDataV1Ptr data);

    static QRect rectFromNormalizedRect(const QRectF& rectF);

    static void addMotion(quint64* dst, const quint64* src);
    static void addMotion(char* dst, const char* src);
    static void addMotion(char* buffer, const QRectF& rectF);
    static void addMotion(char* buffer, const QRect& rect);

    void addMotion(const QRectF& data);
    void addMotion(const QRect& data);

    // Removes part of motion info by motion mask.
    void removeMotion(const simd128i* data);

    static bool isMotionAt(int x, int y, const char* mask);
    static QRect boundingBox(const char* data);

    bool isMotionAt(int x, int y) const;

    void setMotionAt(int x, int y);

    bool mapMotion(const QRect& imageRect, const QRect& mRect);

    bool containTime(const qint64 timeUsec) const override;

    // Returns true if no motion detected.
    bool isEmpty() const;

    /**
     * @param data Should contain Qn::kMotionGridWidth * Qn::kMotionGridHeight / CHAR_BIT bytes.
     */
    void assign(const void* data, qint64 timestamp, qint64 duration);

    static void createMask(
        const QRegion& region,
        char* mask,
        int* maskStart = 0,
        int* maskEnd = 0);
    static void createMask(
        const QRect& rect,
        char* mask,
        int* maskStart = 0,
        int* maskEnd = 0);
    static void createMask(
        const QRectF& rectF,
        char* mask,
        int* maskStart = 0,
        int* maskEnd = 0);

    virtual QnMetaDataV1* clone() const override;

    virtual const char* data() const override;
    char* data();
    virtual size_t dataSize() const override;

    void serialize(QIODevice* ioDevice) const;
    QByteArray serialize() const;

    static bool matchImage(
        const quint64* data,
        const simd128i* mask,
        int maskStart = 0,
        int maskEnd = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 128 - 1);

protected:
    void assign(const QnMetaDataV1* other);

private:
    qint64 m_firstTimestamp = AV_NOPTS_VALUE;
};

using FrameMetadata = QVector<QnAbstractCompressedMetadataPtr>;
