#include "time_manager.h"

#if defined(Q_OS_MACX) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(__aarch64__)
#include <zlib.h>
#else
#include <QtZlib/zlib.h>
#endif

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
    auto serverId = id.toByteArray();
    return crc32(0, (const Bytef*)serverId.constData(), serverId.size());
}

} // namespace legacy
} // namespace time_sync
} // namespace nx
