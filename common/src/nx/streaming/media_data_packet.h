#pragma once

#include <memory>

#include <QtCore/QVector>
#include <QtCore/QRect>

#include <utils/common/byte_array.h>
#include <utils/media/audioformat.h>
#include <utils/media/sse_helper.h>
#include <utils/memory/abstract_allocator.h>
#include <utils/memory/system_allocator.h>
#include <utils/math/math.h>

#include <motion/motion_detection.h>

#include <nx/streaming/aligned_allocator.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/media_context.h>

#include <nx/fusion/model_functions_fwd.h>

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

bool isLowMediaQuality(MediaQuality q);

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

using QnCompressedMetadataPtr = std::shared_ptr<QnCompressedMetadata>;
using QnConstCompressedMetadataPtr = std::shared_ptr<const QnCompressedMetadata>;

using QnMetaDataV1Ptr = std::shared_ptr<QnMetaDataV1>;
using QnConstMetaDataV1Ptr = std::shared_ptr<const QnMetaDataV1>;

using QnMetaDataLightVector = std::vector<QnMetaDataV1Light, QnAlignedAllocator<QnMetaDataV1Light>>;

Q_DECLARE_METATYPE(QnMetaDataV1Ptr);
Q_DECLARE_METATYPE(QnConstMetaDataV1Ptr);
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((MediaQuality), (lexical))

struct QnAbstractMediaData : public QnAbstractDataPacket
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
        // Some data was dropped before current data.
        MediaFlags_AfterDrop            = 0x02000,

        // Hardware decoding is used.
        MediaFlags_HWDecodingUsed       = 0x04000,
        // Ignore syncplay mode.
        MediaFlags_PlayUnsync           = 0x08000,
        // Ignore packet at all.
        MediaFlags_Skip                 = 0x10000,
        // Packet has been delivered via network.
        MediaFlags_GotFromRemotePeer    = 0x20000,
    };

    Q_DECLARE_FLAGS(MediaFlags, MediaFlag)

    enum DataType {
        UNKNOWN = -1,
        VIDEO = 0,
        AUDIO,
        CONTAINER,
        META_V1,
        GENERIC_METADATA,
        EMPTY_DATA
    };

public:
    QnAbstractMediaData(DataType _dataType);
    virtual ~QnAbstractMediaData();

    virtual QnAbstractMediaData* clone(
        QnAbstractAllocator* allocator = QnSystemAllocator::instance()) const = 0;

    virtual const char* data() const = 0;
    virtual size_t dataSize() const = 0;

    bool isLQ() const;
    bool isLive() const;

public:
    DataType dataType;
    AVCodecID compressionType;
    MediaFlags flags;
    // Video or audio channel number. Some devices might have more than one sensor.
    quint32 channelNumber;
    QnConstMediaContextPtr context;
    int opaque;

protected:
    void assign(const QnAbstractMediaData* other);
};

Q_STATIC_ASSERT(AV_PKT_FLAG_KEY == QnAbstractMediaData::MediaFlags_AVKey);
Q_DECLARE_OPERATORS_FOR_FLAGS(QnAbstractMediaData::MediaFlags)

struct QnEmptyMediaData : public QnAbstractMediaData
{

public:
    QnEmptyMediaData();
    QnEmptyMediaData(QnAbstractAllocator* allocator);

    virtual QnEmptyMediaData* clone(
        QnAbstractAllocator* allocator = QnSystemAllocator::instance()) const override;

    virtual const char* data() const override;
    virtual size_t dataSize() const override;

public:
    QnByteArray m_data;
};

enum class MetadataType
{
    Motion,
    ObjectDetection,
    MediaStreamEvent
};

// TODO: #dmishin get rid of implementations and members in this class
// It should be an interface not a base implementation.
struct QnAbstractCompressedMetadata: public QnAbstractMediaData
{

public:
    QnAbstractCompressedMetadata(MetadataType type);

    QnAbstractCompressedMetadata(MetadataType type, QnAbstractAllocator* allocator);

    virtual bool containTime(const qint64 timeUsec) const;

public:
    MetadataType metadataType;
    qint64 m_duration;
    QnByteArray m_data;
};


struct QnCompressedMetadata: public QnAbstractCompressedMetadata
{
    QnCompressedMetadata(MetadataType type);
    QnCompressedMetadata(MetadataType type, QnAbstractAllocator* allocator);

    virtual QnAbstractMediaData* clone(
        QnAbstractAllocator* allocator = QnSystemAllocator::instance()) const override;
    virtual const char* data() const override;
    virtual size_t dataSize() const override;
    virtual bool setData(const char* data, std::size_t dataSize);
};

/**
* This structure used for serialized QnMetaDataV1
* Timestamp and duration specified in milliseconds
* structure can be directly mapped to deserialized memory buffer to represent MetaData
*/
#pragma pack(push, 1)
struct QnMetaDataV1Light
{

public:
    /**
    * Structure MUST be prepared before use by calling doMarshalling method
    */
    void doMarshalling()
    {
        startTimeMs = qFromBigEndian(startTimeMs);
        durationMs = qFromBigEndian(durationMs);
    }

    qint64 endTimeMs() const  { return startTimeMs + durationMs; }

public:
    quint64 startTimeMs;
    quint32 durationMs;
    quint8 channel;
    quint8 input;
    quint16 reserved;
    quint8 data[Qn::kMotionGridWidth*Qn::kMotionGridHeight/8];
};
#pragma pack(pop)

bool operator< (const QnMetaDataV1Light& data, const quint64 timeMs);
bool operator< (const quint64 timeMs, const QnMetaDataV1Light& data);

struct QnMetaDataV1: public QnAbstractCompressedMetadata
{

public:
    QnMetaDataV1(int initialValue = 0);
    QnMetaDataV1(QnAbstractAllocator* allocator, int initialValue = 0);

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

    // Removes part of motion info by motion mask.
    void removeMotion(const simd128i* data);

    static bool isMotionAt(int x, int y, char* mask);
    bool isMotionAt(int x, int y) const;

    void setMotionAt(int x, int y);

    bool mapMotion(const QRect& imageRect, const QRect& mRect);

    bool isInput(int index) const;
    bool containTime(const qint64 timeUsec) const;

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

    virtual QnMetaDataV1* clone(
        QnAbstractAllocator* allocator = QnSystemAllocator::instance()) const override;

    virtual const char* data() const override;
    char* data();
    virtual size_t dataSize() const override;

    void serialize(QIODevice* ioDevice) const;

    static bool matchImage(
        const simd128i* data,
        const simd128i* mask,
        int maskStart = 0,
        int maskEnd = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 128 - 1);

public:
    quint8 m_input;

protected:
    void assign(const QnMetaDataV1* other);

private:
    qint64 m_firstTimestamp;
};
