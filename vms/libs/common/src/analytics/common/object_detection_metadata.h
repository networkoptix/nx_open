#pragma once

#include <set>
#include <chrono>
#include <memory>
#include <ostream>

#include <QtCore/QRect>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/streaming/media_data_packet_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace common {
namespace metadata {

struct Attribute
{
    QString name;
    QString value;
};
#define Attribute_Fields (name)(value)
QN_FUSION_DECLARE_FUNCTIONS(Attribute, (json)(ubjson));

bool operator<(const Attribute& f, const Attribute& s);
bool operator==(const Attribute& left, const Attribute& right);

QString toString(const Attribute&);

//-------------------------------------------------------------------------------------------------
static constexpr int kCoordinateDecimalDigits = 4;

struct ObjectMetadata
{
    QString objectTypeId;
    QnUuid trackId;
    /**
     * Coordinates are in range [0;1].
     */
    QRectF boundingBox;
    std::vector<Attribute> attributes;
    bool bestShot = false;
};
#define ObjectMetadata_Fields (objectTypeId)(trackId)(boundingBox)(attributes)(bestShot)
QN_FUSION_DECLARE_FUNCTIONS(ObjectMetadata, (json)(ubjson));

bool operator==(const ObjectMetadata& left, const ObjectMetadata& right);
QString toString(const ObjectMetadata& object);

//-------------------------------------------------------------------------------------------------

struct ObjectMetadataPacket
{
    QnUuid deviceId;
    qint64 timestampUs = 0;
    qint64 durationUs = 0;
    std::vector<ObjectMetadata> objectMetadataList;
};
#define ObjectMetadataPacket_Fields (deviceId)(timestampUs)(durationUs)(objectMetadataList)
QN_FUSION_DECLARE_FUNCTIONS(ObjectMetadataPacket, (json)(ubjson));

bool operator==(const ObjectMetadataPacket& left, const ObjectMetadataPacket& right);

#define QN_OBJECT_DETECTION_TYPES \
    (Attribute)\
    (ObjectMetadata)\
    (ObjectMetadataPacket)

using ObjectMetadataPacketPtr = std::shared_ptr<ObjectMetadataPacket>;
using ConstObjectMetadataPacketPtr = std::shared_ptr<const ObjectMetadataPacket>;

bool operator<(const ObjectMetadataPacket& first, const ObjectMetadataPacket& second);
bool operator<(std::chrono::microseconds first, const ObjectMetadataPacket& second);
bool operator<(const ObjectMetadataPacket& first, std::chrono::microseconds second);

QString toString(const ObjectMetadataPacket& packet);
QnCompressedMetadataPtr toCompressedMetadataPacket(const ObjectMetadataPacket&);
ObjectMetadataPacketPtr fromCompressedMetadataPacket(const QnConstCompressedMetadataPtr&);
::std::ostream& operator<<(::std::ostream& os, const ObjectMetadataPacket& packet);

} // namespace metadata
} // namespace common
} // namespace nx
