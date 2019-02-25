#pragma once

#include <QtCore/QByteArray>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/subscription.h>

#include "transaction_transport_header.h"
#include "connect_request_attributes.h"
#include "../serialization/serializable_transaction.h"
#include "../serialization/transaction_deserializer.h"

namespace nx::clusterdb::engine::transport {

using ConnectionClosedSubscription =
    nx::utils::Subscription<SystemError::ErrorCode>;

using ConnectionClosedEventHandler =
    typename ConnectionClosedSubscription::NotificationCallback;

using CommandDataHandler = nx::utils::MoveOnlyFunc<void(
    Qn::SerializationFormat,
    const QByteArray&,
    CommandTransportHeader)>;

using CommandHandler = nx::utils::MoveOnlyFunc<void(
    std::unique_ptr<DeserializableCommandData>,
    CommandTransportHeader)>;

//-------------------------------------------------------------------------------------------------

class AbstractCommandPipeline:
    public nx::network::aio::BasicPollable
{
public:
    /**
     * MUST be called to start receiving/sending data.
     */
    virtual void start() = 0;

    virtual network::SocketAddress remotePeerEndpoint() const = 0;

    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) = 0;

    virtual void setOnGotTransaction(CommandDataHandler handler) = 0;

    virtual std::string connectionGuid() const = 0;

    virtual void sendTransaction(
        CommandTransportHeader transportHeader,
        const std::shared_ptr<const CommandSerializer>& transactionSerializer) = 0;

    virtual void closeConnection() = 0;
};

//-------------------------------------------------------------------------------------------------

class AbstractConnection:
    public nx::network::aio::BasicPollable
{
public:
    virtual network::SocketAddress remotePeerEndpoint() const = 0;

    virtual ConnectionClosedSubscription& connectionClosedSubscription() = 0;

    virtual void setOnGotTransaction(CommandHandler handler) = 0;

    virtual std::string connectionGuid() const = 0;

    virtual const CommandTransportHeader& commonTransportHeaderOfRemoteTransaction() const = 0;

    virtual void sendTransaction(
        CommandTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractCommand>& transactionSerializer) = 0;

    virtual void start() = 0;
};

} // namespace nx::clusterdb::engine::transport
