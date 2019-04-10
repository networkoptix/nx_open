#pragma once

#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

QByteArray serialize(const std::vector<long long>& numbers);

void deserialize(
    const QByteArray& serialized,
    std::vector<long long>* numbers);

//-------------------------------------------------------------------------------------------------

class TrackSerializer
{
public:
    /**
     * Two serialization results can be appended to each other safely.
     */
    static QByteArray serialize(const std::vector<ObjectPosition>& /*track*/);

    static std::vector<ObjectPosition> deserialize(const QByteArray& /*serializedData*/);
};

} // namespace nx::analytics::storage
