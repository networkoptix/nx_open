#include "command.h"

#include <nx/utils/log/log_message.h>

namespace nx::clusterdb::engine {

std::string toString(const CommandHeader& header)
{
    return lm("command=%1 time=%2 seq=%3 peer=%4 dbId=%5")
        .args(static_cast<int>(header.command),
            header.persistentInfo.timestamp,
            header.persistentInfo.sequence,
            header.peerID.toString(),
            header.persistentInfo.dbID.toString()).toStdString();
}

} // namespace nx::clusterdb::engine
