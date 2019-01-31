#pragma once

#include <nx/network/buffer.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/log_message.h>

#include "../command.h"

namespace nx::clusterdb::engine {

class CommandTransportHeader
{
public:
    std::string systemId;
    std::string peerId;
    network::SocketAddress endpoint;
    std::string connectionId;
    VmsTransportHeader vmsTransportHeader;
    int transactionFormatVersion;

    CommandTransportHeader(int transactionVersion):
        transactionFormatVersion(transactionVersion)
    {
    }

    QString toString() const
    {
        return lm("(%1.%2; %3)").arg(vmsTransportHeader.sender).arg(systemId).arg(endpoint);
    }
};

} // namespace nx::clusterdb::engine
