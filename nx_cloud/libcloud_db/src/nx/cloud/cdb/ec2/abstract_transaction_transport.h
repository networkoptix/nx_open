#pragma once

#include <QtCore/QByteArray>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>

#include "serialization/serializable_transaction.h"
#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

using ConnectionClosedEventHandler = nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

using GotTransactionEventHandler = nx::utils::MoveOnlyFunc<void(
    Qn::SerializationFormat,
    const QByteArray&,
    TransactionTransportHeader)>;

class AbstractTransactionTransport:
    public nx::network::aio::BasicPollable
{
public:
    virtual ~AbstractTransactionTransport() = default;

    virtual SocketAddress remoteSocketAddr() const = 0;
    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) = 0;
    virtual void setOnGotTransaction(GotTransactionEventHandler handler) = 0;
    virtual QnUuid connectionGuid() const = 0;
    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const = 0;
    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) = 0;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
