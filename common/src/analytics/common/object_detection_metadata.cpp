#include "object_detection_metadata.h"

#include <tuple>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace common {
namespace metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    QN_OBJECT_DETECTION_TYPES,
    (json)(ubjson),
    _Fields,
    (brief, true))

bool operator< (const Attribute& f, const Attribute& s)
{
    return std::tie(f.name, f.value) < std::tie(s.name, s.value);
}

bool operator<(const DetectionMetadataPacket& first, const DetectionMetadataPacket& other)
{
    return first.timestampUsec < other.timestampUsec;
}

bool operator<(std::chrono::microseconds first, const DetectionMetadataPacket& second)
{
    return first.count() < second.timestampUsec;
}

bool operator<(const DetectionMetadataPacket& first, std::chrono::microseconds second)
{
    return first.timestampUsec < second.count();
}

} // namespace metadata
} // namespace common
} // namespace nx
