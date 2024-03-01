// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include <nx/media/abstract_data_packet.h>
#include <nx/media/codec_parameters.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/byte_array.h>

// TODO: #dmishin move all classes to separate source files.
// TODO: #dmishin place this code into proper namespace.

class QIODevice;
class QRegion;
struct AVCodecContext;

enum MediaQuality
{
    MEDIA_Quality_High = 1,
    MEDIA_Quality_Low = 2,
    // At current version MEDIA_Quality_ForceHigh is very similar to MEDIA_Quality_High.
    // It used for export to 'avi' or 'mkv'.
    // This mode do not tries first short LQ chunk if LQ chunk has slightly better position
    MEDIA_Quality_ForceHigh,
    MEDIA_Quality_ForceLow,
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

NX_MEDIA_CORE_API bool isLowMediaQuality(MediaQuality q);

struct QnAbstractMediaData;
struct QnEmptyMediaData;

using QnAbstractMediaDataPtr = std::shared_ptr<QnAbstractMediaData>;
using QnConstAbstractMediaDataPtr = std::shared_ptr<const QnAbstractMediaData>;
using QnEmptyMediaDataPtr = std::shared_ptr<QnEmptyMediaData>;

struct NX_MEDIA_CORE_API QnAbstractMediaData: public QnAbstractDataPacket
{

public:
    NX_REFLECTION_ENUM_IN_CLASS(MediaFlag,
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

        // Media packet from old archive version that should be BOM decoded before decrpyption
        // This flag can be removed in a 5.3
        MediaFlags_BomDecoding         = 0x100000
    )

    Q_DECLARE_FLAGS(MediaFlags, MediaFlag)

    NX_REFLECTION_ENUM_IN_CLASS(DataType,
        UNKNOWN = -1,
        VIDEO = 0,
        AUDIO,
        CONTAINER,
        EMPTY_DATA,
        META_V1, //< Deprecated. Don't use it. Use MetadataType instead.
        GENERIC_METADATA
    )

public:
    QnAbstractMediaData(DataType _dataType);
    virtual ~QnAbstractMediaData();

    virtual QnAbstractMediaData* clone() const = 0;

    virtual const char* data() const = 0;
    virtual size_t dataSize() const = 0;

    virtual void setData(nx::utils::ByteArray&& buffer) = 0;

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

AVMediaType NX_MEDIA_CORE_API toAvMediaType(QnAbstractMediaData::DataType dataType);

Q_STATIC_ASSERT(AV_PKT_FLAG_KEY == QnAbstractMediaData::MediaFlags_AVKey);
Q_DECLARE_OPERATORS_FOR_FLAGS(QnAbstractMediaData::MediaFlags)

struct NX_MEDIA_CORE_API QnEmptyMediaData: public QnAbstractMediaData
{

public:
    QnEmptyMediaData();

    virtual QnEmptyMediaData* clone() const override;

    virtual const char* data() const override;
    virtual size_t dataSize() const override;

    virtual void setData(nx::utils::ByteArray&& buffer) override;

public:
    nx::utils::ByteArray m_data;
};
