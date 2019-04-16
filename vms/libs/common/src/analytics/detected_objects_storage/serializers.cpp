#include "serializers.h"

namespace nx::analytics::storage {

QByteArray serialize(const std::vector<long long>& /*numbers*/)
{
    // TODO
    return QByteArray();
}

void deserialize(
    const QByteArray& /*serialized*/,
    std::vector<long long>* /*numbers*/)
{
    // TODO
}

//-------------------------------------------------------------------------------------------------

QByteArray TrackSerializer::serialize(const std::vector<ObjectPosition>& /*track*/)
{
    NX_ASSERT(false);
    // TODO
    return QByteArray();
}

std::vector<ObjectPosition> TrackSerializer::deserialize(
    const QByteArray& /*serializedData*/)
{
    // TODO
    return {};
}

} // namespace nx::analytics::storage
