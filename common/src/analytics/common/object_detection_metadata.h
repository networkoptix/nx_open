#pragma once

#include <set>
#include <chrono>

#include <analytics/common/abstract_metadata.h>

#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>

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

struct DetectedObject
{
    QnUuid objectId;
    QRectF boundingBox;
    std::vector<Attribute> labels;
};
#define DetectedObject_Fields (objectId)(boundingBox)(labels)
QN_FUSION_DECLARE_FUNCTIONS(DetectedObject, (json)(ubjson));

struct DetectionMetadataPacket
{
    qint64 timestampUsec = 0;
    qint64 durationUsec = 0;
    std::vector<DetectedObject> objects;
};
#define DetectionMetadataPacket_Fields (timestampUsec)(durationUsec)(objects)
QN_FUSION_DECLARE_FUNCTIONS(DetectionMetadataPacket, (json)(ubjson));

#define QN_OBJECT_DETECTION_TYPES \
    (Attribute)\
    (DetectedObject)\
    (DetectionMetadataPacket)

using DetectionMetadataPacketPtr = std::shared_ptr<DetectionMetadataPacket>;
using DetectionMetadataTrack = DetectionMetadataPacket;

bool operator<(const DetectionMetadataPacket& first,
    const DetectionMetadataPacket& second);
bool operator<(std::chrono::microseconds first, const DetectionMetadataPacket& second);
bool operator<(const DetectionMetadataPacket& first, std::chrono::microseconds second);


} // namespace metadata
} // namespace common
} // namespace nx
