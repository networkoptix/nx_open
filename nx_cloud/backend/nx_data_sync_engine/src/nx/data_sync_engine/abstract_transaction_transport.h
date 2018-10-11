#pragma once

#include <QtCore/QByteArray>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/subscription.h>

#include "serialization/serializable_transaction.h"
#include "transaction_transport_header.h"

namespace nx {
namespace data_sync_engine {

using ConnectionClosedSubscription =
    nx::utils::Subscription<SystemError::ErrorCode>;

using ConnectionClosedEventHandler =
    typename ConnectionClosedSubscription::NotificationCallback;

using GotTransactionEventHandler = nx::utils::MoveOnlyFunc<void(
    Qn::SerializationFormat,
    const QByteArray&,
    TransactionTransportHeader)>;

struct ConnectionRequestAttributes
{
    std::string connectionId;
    vms::api::PeerData remotePeer;
    std::string contentEncoding;
    int remotePeerProtocolVersion = 0;
};

//-------------------------------------------------------------------------------------------------

class AbstractCommandPipeline:
    public nx::network::aio::BasicPollable
{
public:
    virtual network::SocketAddress remotePeerEndpoint() const = 0;

    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) = 0;

    virtual void setOnGotTransaction(GotTransactionEventHandler handler) = 0;

    virtual QnUuid connectionGuid() const = 0;

    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const TransactionSerializer>& transactionSerializer) = 0;

    virtual void closeConnection() = 0;
};

//-------------------------------------------------------------------------------------------------

class AbstractTransactionTransport:
    public nx::network::aio::BasicPollable
{
public:
    virtual network::SocketAddress remotePeerEndpoint() const = 0;

    virtual ConnectionClosedSubscription& connectionClosedSubscription() = 0;

    virtual void setOnGotTransaction(GotTransactionEventHandler handler) = 0;

    virtual QnUuid connectionGuid() const = 0;

    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const = 0;

    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) = 0;

    virtual void start() = 0;
};

} // namespace data_sync_engine
} // namespace nx
