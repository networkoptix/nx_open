#pragma once

#include <nx/network/buffer.h>
#include <nx/network/socket_common.h>
#include <nx/utils/log/log_message.h>

#include <transaction/transaction_transport_header.h>

namespace nx {
namespace cdb {
namespace ec2 {

class TransactionTransportHeader
{
public:
    nx::String systemId;
    SocketAddress endpoint;
    nx::String connectionId;
    ::ec2::QnTransactionTransportHeader vmsTransportHeader;

    QString toString() const
    {
        return lm("(%1.%2; %3)").str(vmsTransportHeader.sender).arg(systemId).str(endpoint);
    }
};

} // namespace ec2
} // namespace cdb
} // namespace nx
