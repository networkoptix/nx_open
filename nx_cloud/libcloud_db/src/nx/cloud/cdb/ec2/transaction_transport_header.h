#pragma once

#include <nx_ec/ec_proto_version.h>
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
    network::SocketAddress endpoint;
    nx::String connectionId;
    ::ec2::QnTransactionTransportHeader vmsTransportHeader;
    int transactionFormatVersion;

    TransactionTransportHeader():
        transactionFormatVersion(nx_ec::EC2_PROTO_VERSION)
    {
    }

    QString toString() const
    {
        return lm("(%1.%2; %3)").arg(vmsTransportHeader.sender).arg(systemId).arg(endpoint);
    }
};

} // namespace ec2
} // namespace cdb
} // namespace nx
