// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <list>
#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/elapsed_timer.h>

#include "connector_factory.h"

namespace nx::network::cloud {

/**
 * Responsible for establishing connection to the target server AFTER successful communication
 * with the connection_mediator. It does not communicate with the connection_mediator; this is done
 * by the CrossNatConnector. This class manages different connector types: [tcp|udp] direct connection
 * or via proxy (traffic_relay).
*/
class NX_NETWORK_API ConnectorExecutor:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(TunnelConnectResult)>;

    ConnectorExecutor(
        const AddressEntry& targetAddress,
        const std::string& connectSessionId,
        const hpm::api::ConnectResponse& response,
        std::unique_ptr<AbstractDatagramSocket> udpSocket);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void setTimeout(std::chrono::milliseconds timeout);
    void start(CompletionHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct ConnectorContext
    {
        TunnelConnectorContext tunnelCtx;
        std::unique_ptr<aio::Timer> startDelayTimer;
        TunnelConnectStatistics stats;
    };

    const std::string m_connectSessionId;
    hpm::api::ConnectResponse m_response;
    std::list<ConnectorContext> m_connectors;
    std::chrono::milliseconds m_connectTimeout{0};
    CompletionHandler m_handler;

    void reportNoSuitableConnectMethod();

    void startConnectors();

    void startConnector(std::list<ConnectorContext>::iterator connectorIter);

    void onConnectorFinished(
        std::list<ConnectorContext>::iterator connectorIter,
        TunnelConnectResult result);
};

} // namespace nx::network::cloud
