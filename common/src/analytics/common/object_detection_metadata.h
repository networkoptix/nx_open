#pragma once

#include <set>

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
QN_FUSION_DECLARE_FUNCTIONS(Attribute, (json)(ubjson)(metatype));
#define Attribute_Fields (name)(value)

struct DetectedObject
{
    QnUuid objectId;
    QRectF boundingBox;
    std::vector<Attribute> labels;
};
QN_FUSION_DECLARE_FUNCTIONS(DetectedObject, (json)(ubjson)(metatype));
#define DetectedObject_Fields (objectId)(boundingBox)(labels)

struct DetectionMetadataPacket
{
    qint64 timestampUsec = 0;
    qint64 durationUsec = 0;
    std::vector<DetectedObject> objects;
};
QN_FUSION_DECLARE_FUNCTIONS(DetectionMetadataPacket, (json)(ubjson)(metatype));
#define DetectionMetadataPacket_Fields (timestampUsec)(durationUsec)(objects)

#define QN_OBJECT_DETECTION_TYPES \
    (Attribute)\
    (DetectedObject)\
    (DetectionMetadataPacket)

using DetectionMetadataPacketPtr = std::shared_ptr<DetectionMetadataPacket>;


} // namespace metadata
} // namespace common
} // namespace nx
