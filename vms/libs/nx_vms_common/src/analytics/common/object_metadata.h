// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <ostream>
#include <set>

#include <QtCore/QRect>
#include <QtCore/QVector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/media/meta_data_packet.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx {
namespace common {
namespace metadata {

/**%apidoc Analytics object attribute. */
struct NX_VMS_COMMON_API Attribute
{
    /**%apidoc Attribute name. */
    QString name;

    /**%apidoc Attribute value. */
    QString value;

    Attribute() = default;
    Attribute(const QString& name, const QString& value):
        name(name), value(value)
    {
    };
};
#define Attribute_Fields (name)(value)
QN_FUSION_DECLARE_FUNCTIONS(Attribute, (json)(ubjson)(xml)(csv_record), NX_VMS_COMMON_API);

NX_REFLECTION_INSTRUMENT(Attribute, Attribute_Fields)

NX_VMS_COMMON_API bool operator<(const Attribute& f, const Attribute& s);
NX_VMS_COMMON_API bool operator==(const Attribute& left, const Attribute& right);

NX_VMS_COMMON_API QString toString(const Attribute&);

using Attributes = std::vector<Attribute>;

NX_VMS_COMMON_API void addAttributeIfNotExists(Attributes* result, const Attribute& a);
NX_VMS_COMMON_API Attributes::iterator findFirstAttributeByName(Attributes* a, const QString& name);
NX_VMS_COMMON_API Attributes::const_iterator findFirstAttributeByName(
    const Attributes* a, const QString& name);

struct RangePoint
{
    float value = 0.0;
    bool inclusive = false;
};

struct NX_VMS_COMMON_API NumericRange
{
    NumericRange() = default;
    NumericRange(float value):
        from(RangePoint{value, true}),
        to(RangePoint{value, true})
    {
    }

    NumericRange(std::optional<RangePoint> from, std::optional<RangePoint> to):
        from(std::move(from)), to(std::move(to))
    {
    }

    bool intersects(const NumericRange& range) const;
    bool hasRange(const NumericRange& range) const;
    QString stringValue() const;
    bool isNull() const { return !from && !to; }

    std::optional<RangePoint> from;
    std::optional<RangePoint> to;
};

struct NX_VMS_COMMON_API AttributeEx
{
    QString name;
    std::variant<QString, NumericRange> value;

    AttributeEx() = default;
    AttributeEx(const Attribute& attribute);

    static NumericRange parseRangeFromValue(const QString& value);
    static bool isNumberOrRange(const QString& attributeName, const QString& attributeValue);

    void addRange(const NumericRange& range);
    QString stringValue() const;
};

struct AttributeGroup
{
    QString name;
    QStringList values;
};

NX_VMS_COMMON_API bool operator==(const AttributeGroup& left, const AttributeGroup& right);

using GroupedAttributes = QVector<AttributeGroup>; //< QVector for implicit sharing in the UI.

NX_VMS_COMMON_API GroupedAttributes groupAttributes(
    const Attributes& attributes,
    bool humanReadableNames = true, //< If true, replaces dots with spaces.
    bool filterOutHidden = true);

//-------------------------------------------------------------------------------------------------
static constexpr int kCoordinateDecimalDigits = 4;

NX_REFLECTION_ENUM_CLASS(ObjectMetadataType,
    /**%apidoc Undefined analytics object. */
    undefined,

    /**%apidoc Regular analytics object. It represents a rectangle where object was detected. */
    regular,

    /**%apidoc It points to the best recognized rectangle of the whole track.
     * This time `boundingBox` can be used to represent track thumbnail.
     */
    bestShot,

    /**%apidoc same as `bestShot` but such metadata has explicitly loaded image from a Device.
     * The API request /ec2/analyticsTrackBestShot uses this data.
     */
    externalBestShot
)

// TODO: #rvasilenko: This struct should NOT be used bot best shots, because it was originally
// designed to match IObjectMetadata, and best shots are yielded by a plugin as
// IObjectTrackBestShotPacket which has no relation to IObjectMetadata.
struct NX_VMS_COMMON_API ObjectMetadata
{
    /**%apidoc
     * Object type.
     * %example car
     */
    QString typeId;

    /**%apidoc Unique track Id. */
    QnUuid trackId;

    /**%apidoc Bounding box of the object. JSON object containing top-left corner (fields `x`
     * and `y`), `width` and `height`. Relative coordinates are in range [0;1].
     */
    QRectF boundingBox;

    std::vector<Attribute> attributes;

    /**%apidoc Kind of the object. */
    ObjectMetadataType objectMetadataType;

    /**%apidoc Unique Id of the analytics engine. */
    QnUuid analyticsEngineId;

    /**%apidoc Image url, only for Best Shot packets. */
    QString imageUrl;

    /**%apidoc Image data format, only for Best Shot packets. */
    QString imageDataFormat;

    /**%apidoc Image data size, only for Best Shot packets. */
    int imageDataSize;

    /**%apidoc Whether the image data is not null, only for Best Shot packets. */
    bool isImageDataPresent;

    bool isBestShot() const
    {
        return objectMetadataType == ObjectMetadataType::bestShot
            || objectMetadataType == ObjectMetadataType::externalBestShot;
    }

    bool isExternalBestShot() const
    {
        return objectMetadataType == ObjectMetadataType::externalBestShot;
    }
};
#define ObjectMetadata_Fields \
    (typeId) \
    (trackId) \
    (boundingBox) \
    (attributes) \
    (objectMetadataType) \
    (analyticsEngineId)

QN_FUSION_DECLARE_FUNCTIONS(ObjectMetadata, (json)(ubjson), NX_VMS_COMMON_API);

NX_VMS_COMMON_API bool operator==(const ObjectMetadata& left, const ObjectMetadata& right);
NX_VMS_COMMON_API QString toString(const ObjectMetadata& object);

NX_REFLECTION_INSTRUMENT(ObjectMetadata, ObjectMetadata_Fields)

//-------------------------------------------------------------------------------------------------

/**%apidoc Analytics metadata packet. */
struct NX_VMS_COMMON_API ObjectMetadataPacket
{
    /**%apidoc Device id. */
    QnUuid deviceId;

    /**%apidoc UTC timestamp in microseconds. */
    qint64 timestampUs = 0;

    /**%apidoc[opt] Duration of the object in microseconds. */
    qint64 durationUs = 0;

    std::vector<ObjectMetadata> objectMetadataList;

    /**%apidoc[opt] Video stream from which analytics data was received. */
    nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::undefined;

    bool containsBestShotMetadata() const
    {
        return std::any_of(
            objectMetadataList.cbegin(),
            objectMetadataList.cend(),
            [](const ObjectMetadata& objectMetadata) { return objectMetadata.isBestShot(); });
    }

    bool containsExternalBestShotMetadata() const
    {
        return std::any_of(
            objectMetadataList.cbegin(),
            objectMetadataList.cend(),
            [](const ObjectMetadata& objectMetadata)
            {
                return objectMetadata.isExternalBestShot();
            });
    }
};

#define ObjectMetadataPacket_Fields (deviceId) \
    (timestampUs) \
    (durationUs) \
    (objectMetadataList) \
    (streamIndex)

QN_FUSION_DECLARE_FUNCTIONS(ObjectMetadataPacket, (json)(ubjson), NX_VMS_COMMON_API);

NX_REFLECTION_INSTRUMENT(ObjectMetadataPacket, ObjectMetadataPacket_Fields)

NX_VMS_COMMON_API bool operator==(const ObjectMetadataPacket& left, const ObjectMetadataPacket& right);

using ObjectMetadataPacketPtr = std::shared_ptr<ObjectMetadataPacket>;
using ConstObjectMetadataPacketPtr = std::shared_ptr<const ObjectMetadataPacket>;

NX_VMS_COMMON_API bool operator<(
    const ObjectMetadataPacket& first,
    const ObjectMetadataPacket& second);
NX_VMS_COMMON_API bool operator<(std::chrono::microseconds first, const ObjectMetadataPacket& second);
NX_VMS_COMMON_API bool operator<(const ObjectMetadataPacket& first, std::chrono::microseconds second);

NX_VMS_COMMON_API QString toString(const ObjectMetadataPacket& packet);

/**
 * Contains the binary data for the records from ObjectMetadataPacket which have the same Object
 * Type.
 */
class NX_VMS_COMMON_API QnCompressedObjectMetadataPacket: public QnCompressedMetadata
{
public:
    using base_type = QnCompressedMetadata;

    using QnCompressedMetadata::QnCompressedMetadata;

    virtual QnAbstractMediaData* clone() const override;

    /**
     * Reference to the source (non-compressed) packet. Multiple compressed packets can share
     * the same non-compressed source packet, when they are formed via splitting the source packet
     * by Object Types.
     */
    ConstObjectMetadataPacketPtr packet;

private:
    void assign(const QnCompressedObjectMetadataPacket* other);
};
using QnCompressedObjectMetadataPacketPtr = std::shared_ptr<QnCompressedObjectMetadataPacket>;
using QnConstCompressedObjectMetadataPacketPtr = std::shared_ptr<const QnCompressedObjectMetadataPacket>;

NX_VMS_COMMON_API ObjectMetadataPacketPtr fromCompressedMetadataPacket(
    const QnConstCompressedMetadataPtr&);
NX_VMS_COMMON_API ::std::ostream& operator<<(::std::ostream& os, const ObjectMetadataPacket& packet);

} // namespace metadata
} // namespace common
} // namespace nx
