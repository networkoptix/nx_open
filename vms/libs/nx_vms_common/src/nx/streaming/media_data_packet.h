// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QVector>
#include <QtCore/QRect>

#include <utils/common/byte_array.h>
#include <utils/media/sse_helper.h>

#include <motion/motion_detection.h>

#include <nx/streaming/aligned_allocator.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>

#include <nx/reflect/enum_instrument.h>
#include <nx/vms/common/globals/media_stream_event.h>

#include "media_data_packet_fwd.h"
#include <nx/vms/api/types/resource_types.h>

// TODO: #dmishin move all classes to separate source files.
// TODO: #dmishin place this code into proper namespace.

class QIODevice;
struct AVCodecContext;

enum MediaQuality
{
    MEDIA_Quality_High = 1,
    MEDIA_Quality_Low = 2,
    // At current version MEDIA_Quality_ForceHigh is very similar to MEDIA_Quality_High.
    // It used for export to 'avi' or 'mkv'.
    // This mode do not tries first short LQ chunk if LQ chunk has slightly better position
    MEDIA_Quality_ForceHigh,
    MEDIA_Quality_Auto,
    MEDIA_Quality_CustomResolution,
    MEDIA_Quality_LowIframesOnly,
    MEDIA_Quality_None
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(MediaQuality*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<MediaQuality>;
    return visitor(
        Item{MEDIA_Quality_High, "high"},
        Item{MEDIA_Quality_Low, "low"},
        Item{MEDIA_Quality_ForceHigh, "force-high"},
        Item{MEDIA_Quality_Auto, "auto"},
        Item{MEDIA_Quality_CustomResolution, "custom"},
        Item{MEDIA_Quality_LowIframesOnly, "low-iframes-only"},
        Item{MEDIA_Quality_None, ""}
    );
}

NX_VMS_COMMON_API bool isLowMediaQuality(MediaQuality q);

struct QnAbstractMediaData;
struct QnEmptyMediaData;
struct QnAbstractCompressedMetadata;
struct QnCompressedMetadata;
struct QnMetaDataV1;
struct QnMetaDataV1Light;

using QnAbstractMediaDataPtr = std::shared_ptr<QnAbstractMediaData>;
using QnConstAbstractMediaDataPtr = std::shared_ptr<const QnAbstractMediaData>;

using QnEmptyMediaDataPtr = std::shared_ptr<QnEmptyMediaData>;

using QnAbstractCompressedMetadataPtr = std::shared_ptr<QnAbstractCompressedMetadata>;
using QnConstAbstractCompressedMetadataPtr = std::shared_ptr<const QnAbstractCompressedMetadata>;

using QnMetaDataV1Ptr = std::shared_ptr<QnMetaDataV1>;
using QnConstMetaDataV1Ptr = std::shared_ptr<const QnMetaDataV1>;

using QnMetaDataLightVector = std::vector<QnMetaDataV1Light, QnAlignedAllocator<QnMetaDataV1Light>>;

Q_DECLARE_METATYPE(QnMetaDataV1Ptr);
Q_DECLARE_METATYPE(QnConstMetaDataV1Ptr);
Q_DECLARE_METATYPE(QnAbstractCompressedMetadataPtr);

struct NX_VMS_COMMON_API QnAbstractMediaData: public QnAbstractDataPacket
{

public:
    enum MediaFlag
    {
        MediaFlags_None                 = 0x00000,
        // KeyFrame, must be equal to AV_PKT_FLAG_KEY from avcodec.h,
        // checked via static_assert below.
        MediaFlags_AVKey                = 0x00001,
        MediaFlags_AfterEOF             = 0x00002,
        MediaFlags_BOF                  = 0x00004,
        MediaFlags_LIVE                 = 0x00008,
        // The frame should not be displayed.
        MediaFlags_Ignore               = 0x00010,

        MediaFlags_ReverseReordered     = 0x00020,
        MediaFlags_ReverseBlockStart    = 0x00040,
        MediaFlags_Reverse              = 0x00080,

        MediaFlags_LowQuality           = 0x00100,
        MediaFlags_StillImage           = 0x00200,

        // Switch archive to a new server.
        MediaFlags_NewServer            = 0x00400,
        MediaFlags_DecodeTwice          = 0x00800,
        // Fast channel zapping flag.
        MediaFlags_FCZ                  = 0x01000,

        MediaFlags_Reserved1            = 0x02000,

        // Hardware decoding is used.
        MediaFlags_HWDecodingUsed       = 0x04000,
        // Ignore syncplay mode.
        MediaFlags_PlayUnsync           = 0x08000,
        // Ignore packet at all.
        MediaFlags_Skip                 = 0x10000,
        MediaFlags_AlwaysSave           = 0x20000,

        // The only intention for packets with such a flag is to be shown to the user in the Live
        // mode. They are not going to be recorded.
        MediaFlags_LiveOnly             = 0x40000,

        // Best shot packets. They can be pipelined with some delay from live. Will not be recorded
        // to mkv files. Will not be reordered in AbstractDataReorderer.
        MediaFlags_MetaDataBestShot     = 0x80000,
    };

    Q_DECLARE_FLAGS(MediaFlags, MediaFlag)

    enum DataType
    {
        UNKNOWN = -1,
        VIDEO = 0,
        AUDIO,
        CONTAINER,
        EMPTY_DATA,
        META_V1, //< Deprecated. Don't use it. Use MetadataType instead.
        GENERIC_METADATA,
    };

public:
    QnAbstractMediaData(DataType _dataType);
    virtual ~QnAbstractMediaData();

    virtual QnAbstractMediaData* clone() const = 0;

    virtual const char* data() const = 0;
    virtual size_t dataSize() const = 0;

    virtual void setData(QnByteArray&& buffer) = 0;

    bool isLQ() const;
    bool isLive() const;

public:
    QnAbstractStreamDataProvider* dataProvider;
    DataType dataType;
    AVCodecID compressionType;
    MediaFlags flags;
    // Video or audio channel number. Some devices might have more than one sensor.
    quint32 channelNumber;
    CodecParametersConstPtr context;
    int opaque;
    std::vector<uint8_t> encryptionData; //< Its non-empty in case of packet is encrypted.
protected:
    void assign(const QnAbstractMediaData* other);
};

AVMediaType toAvMediaType(QnAbstractMediaData::DataType dataType);

Q_STATIC_ASSERT(AV_PKT_FLAG_KEY == QnAbstractMediaData::MediaFlags_AVKey);
Q_DECLARE_OPERATORS_FOR_FLAGS(QnAbstractMediaData::MediaFlags)

struct NX_VMS_COMMON_API QnEmptyMediaData: public QnAbstractMediaData
{

public:
    QnEmptyMediaData();

    virtual QnEmptyMediaData* clone() const override;

    virtual const char* data() const override;
    virtual size_t dataSize() const override;

    virtual void setData(QnByteArray&& buffer) override;

public:
    QnByteArray m_data;
};

enum class MetadataType
{
    Motion,
    ObjectDetection,
    MediaStreamEvent,
    InStream
};

// TODO: #dmishin get rid of implementations and members in this class
// It should be an interface not a base implementation.
struct NX_VMS_COMMON_API QnAbstractCompressedMetadata: public QnAbstractMediaData
{

public:
    QnAbstractCompressedMetadata(MetadataType type, size_t bufferSize);

    virtual bool containTime(const qint64 timeUsec) const;

    qint64 duration() const;

    virtual void setData(QnByteArray&& buffer) override;

public:
    MetadataType metadataType;
    qint64 m_duration = 0;
    QnByteArray m_data;
};

struct NX_VMS_COMMON_API QnCompressedMetadata: public QnAbstractCompressedMetadata
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
        nx::vms::common::MediaStreamEvent value,
        const QByteArray& extraData = QByteArray());

    nx::vms::common::MediaStreamEventPacket toMediaEventPacket() const;

protected:
    void assign(const QnAbstractCompressedMetadata* other);
};

/**
* This structure used for serialized QnMetaDataV1
* Timestamp and duration specified in milliseconds
* structure can be directly mapped to deserialized memory buffer to represent MetaData
*/
#pragma pack(push, 1)
struct NX_VMS_COMMON_API QnMetaDataV1Light
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

bool operator< (const QnMetaDataV1Light& data, const quint64 timeMs);
bool operator< (const quint64 timeMs, const QnMetaDataV1Light& data);

struct NX_VMS_COMMON_API QnMetaDataV1: public QnAbstractCompressedMetadata
{
public:
    static const int kMotionDataBufferSize = Qn::kMotionGridWidth*Qn::kMotionGridHeight / 8;

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
