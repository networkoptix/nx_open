#pragma once

#include <nx/utils/uuid.h>

namespace nx {
namespace time_sync {
namespace legacy {

class TimePriorityKey
{
public:
    //!sequence number. Incremented with each peer selection by user
    quint16 sequence = 0;
    //!bitset of flags from \a TimeSynchronizationManager class
    quint16 flags = 0;
    //!some random number
    quint32 seed = 0;

  
    void fromUInt64(quint64 val);
    static quint32 seedFromId(const QnUuid& id);
};

} // namespace legacy
} // namespace time_sync
} // namespace nx
