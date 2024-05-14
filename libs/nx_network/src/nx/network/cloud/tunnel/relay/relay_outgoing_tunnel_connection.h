// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>
#include <memory>
#include <optional>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_delegate.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/thread/mutex.h>

#include "../abstract_outgoing_tunnel_connection.h"
#include "api/relay_api_client.h"

namespace nx::network::cloud::relay {

class NX_NETWORK_API OutgoingTunnelConnection:
    public AbstractOutgoingTunnelConnection
{
    using base_type = AbstractOutgoingTunnelConnection;

public:
    OutgoingTunnelConnection(
        nx::utils::Url relayUrl,
        std::string relaySessionId,
        std::unique_ptr<nx::cloud::relay::api::AbstractClient> relayApiClient);

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void start() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;

    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual ConnectType connectType() const override;

    virtual std::string toString() const override;

    void setInactivityTimeout(std::chrono::milliseconds timeout);

private:
    struct RequestContext
    {
        std::unique_ptr<nx::cloud::relay::api::AbstractClient> relayClient;
        SocketAttributes socketAttributes;
        OnNewConnectionHandler completionHandler;
        nx::network::aio::Timer timer;
    };

    const nx::utils::Url m_relayUrl;
    const std::string m_relaySessionId;
    std::unique_ptr<nx::cloud::relay::api::AbstractClient> m_relayApiClient;
    nx::Mutex m_mutex;
    std::list<std::unique_ptr<RequestContext>> m_activeRequests;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_tunnelClosedHandler;
    utils::InterruptionFlag m_objectDestructionFlag;
    std::optional<std::chrono::milliseconds> m_inactivityTimeout;
    nx::network::aio::Timer m_inactivityTimer;
    std::shared_ptr<int> m_usageCounter;

    void startInactivityTimer();
    void stopInactivityTimer();
    void onConnectionOpened(
        nx::cloud::relay::api::ResultCode resultCode,
        std::unique_ptr<AbstractStreamSocket> connection,
        std::list<std::unique_ptr<RequestContext>>::iterator requestIter);
    void reportTunnelClosure(SystemError::ErrorCode reason);
    void onInactivityTimeout();
};

class OutgoingConnection:
    public StreamSocketDelegate
{
public:
    OutgoingConnection(
        std::unique_ptr<AbstractStreamSocket> delegate,
        std::shared_ptr<int> usageCounter);
    virtual ~OutgoingConnection() override;

    virtual bool getProtocol(int* protocol) const override;

private:
    std::unique_ptr<AbstractStreamSocket> m_delegate;
    std::shared_ptr<int> m_usageCounter;
};

} // namespace nx::network::cloud::relay
