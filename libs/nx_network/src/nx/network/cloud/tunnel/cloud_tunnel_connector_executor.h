// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <list>
#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include "connector_factory.h"

namespace nx::network::cloud {

class NX_NETWORK_API ConnectorExecutor:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(
        nx::hpm::api::NatTraversalResultCode /*resultCode*/,
        SystemError::ErrorCode /*sysErrorCode*/,
        std::unique_ptr<AbstractOutgoingTunnelConnection> /*connection*/)>;

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
        std::unique_ptr<AbstractTunnelConnector> connector;
        std::chrono::milliseconds startDelay{0};
        std::unique_ptr<aio::Timer> timer;
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
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection);
};

} // namespace nx::network::cloud
