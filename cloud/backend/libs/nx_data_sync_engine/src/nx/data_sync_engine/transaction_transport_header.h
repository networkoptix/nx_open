#pragma once

#include <nx/network/buffer.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/log_message.h>

#include "command.h"

namespace nx::data_sync_engine {

class TransactionTransportHeader
{
public:
    std::string systemId;
    std::string peerId;
    network::SocketAddress endpoint;
    std::string connectionId;
    CommandTransportHeader vmsTransportHeader;
    int transactionFormatVersion;

    TransactionTransportHeader(int transactionVersion):
        transactionFormatVersion(transactionVersion)
    {
    }

    QString toString() const
    {
        return lm("(%1.%2; %3)").arg(vmsTransportHeader.sender).arg(systemId).arg(endpoint);
    }
};

} // namespace nx::data_sync_engine
