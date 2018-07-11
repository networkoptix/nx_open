#include "time_manager.h"

#include <nx/utils/crc32.h>

namespace nx {
namespace time_sync {
namespace legacy {

void TimePriorityKey::fromUInt64(quint64 val)
{
    sequence = (quint16)(val >> 48);
    flags = (quint16)((val >> 32) & 0xFFFF);
    seed = val & 0xFFFFFFFF;
}

quint32 TimePriorityKey::seedFromId(const QnUuid& id)
{
    return nx::utils::crc32(id.toByteArray());
}

} // namespace legacy
} // namespace time_sync
} // namespace nx
