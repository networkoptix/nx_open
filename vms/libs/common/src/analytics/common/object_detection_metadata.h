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

bool operator< (const Attribute& f, const Attribute& s);
bool operator==(const Attribute& left, const Attribute& right);

QString toString(const Attribute&);

//-------------------------------------------------------------------------------------------------

// TODO: Rename all classes without words "Detection" and "Detected".

static constexpr int kCoordinateDecimalDigits = 4;

struct DetectedObject
{
    QString objectTypeId;
    QnUuid objectId;
    /**
     * Coordinates are in range [0;1].
     */
    QRectF boundingBox;
    std::vector<Attribute> labels;
};
#define DetectedObject_Fields (objectTypeId)(objectId)(boundingBox)(labels)
QN_FUSION_DECLARE_FUNCTIONS(DetectedObject, (json)(ubjson));

bool operator==(const DetectedObject& left, const DetectedObject& right);

//-------------------------------------------------------------------------------------------------

struct DetectionMetadataPacket
{
    QnUuid deviceId;
    // TODO: Rename "Usec" -> "Us".
    qint64 timestampUsec = 0;
    qint64 durationUsec = 0;
    std::vector<DetectedObject> objects;
};
#define DetectionMetadataPacket_Fields (deviceId)(timestampUsec)(durationUsec)(objects)
QN_FUSION_DECLARE_FUNCTIONS(DetectionMetadataPacket, (json)(ubjson));

bool operator==(const DetectionMetadataPacket& left, const DetectionMetadataPacket& right);

#define QN_OBJECT_DETECTION_TYPES \
    (Attribute)\
    (DetectedObject)\
    (DetectionMetadataPacket)

using DetectionMetadataPacketPtr = std::shared_ptr<DetectionMetadataPacket>;
using ConstDetectionMetadataPacketPtr = std::shared_ptr<const DetectionMetadataPacket>;
using DetectionMetadataTrack = DetectionMetadataPacket;

bool operator<(const DetectionMetadataPacket& first,
    const DetectionMetadataPacket& second);
bool operator<(std::chrono::microseconds first, const DetectionMetadataPacket& second);
bool operator<(const DetectionMetadataPacket& first, std::chrono::microseconds second);

QString toString(const DetectionMetadataPacket&);
QnCompressedMetadataPtr toMetadataPacket(const DetectionMetadataPacket&);
DetectionMetadataPacketPtr fromMetadataPacket(const QnConstCompressedMetadataPtr&);
::std::ostream& operator<<(::std::ostream& os, const DetectionMetadataPacket& packet);

} // namespace metadata
} // namespace common
} // namespace nx
