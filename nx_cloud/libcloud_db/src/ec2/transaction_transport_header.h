/**********************************************************
* Aug 15, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/network/buffer.h>
#include <nx/network/socket_common.h>
#include <transaction/transaction_transport_header.h>


namespace nx {
namespace cdb {
namespace ec2 {

class TransactionTransportHeader
{
public:
    nx::String systemId;
    SocketAddress endpoint;
    ::ec2::QnTransactionTransportHeader vmsTransportHeader;
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
