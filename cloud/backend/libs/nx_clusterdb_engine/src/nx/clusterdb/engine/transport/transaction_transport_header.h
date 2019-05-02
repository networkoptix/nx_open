#pragma once

#include <nx/network/buffer.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/log_message.h>

#include <transaction/transaction_transport_header.h>

#include "../command.h"

namespace nx::clusterdb::engine {

// TODO: #ak Remove this type.
using VmsTransportHeader = ::ec2::QnTransactionTransportHeader;

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
