// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/aio/timer.h>
#include <nx/utils/elapsed_timer.h>

#include "../abstract_tunnel_connector.h"
#include "api/relay_api_client.h"

namespace nx::network::cloud::relay {

class NX_NETWORK_API Connector:
    public AbstractTunnelConnector
{
    using base_type = AbstractTunnelConnector;

public:
    Connector(
        nx::utils::Url relayUrl,
        AddressEntry targetHostAddress,
        std::string connectSessionId);
    virtual ~Connector() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual int getPriority() const override;
    virtual void connect(
        const hpm::api::ConnectResponse& response,
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

private:
    const nx::utils::Url m_relayUrl;
    const AddressEntry m_targetHostAddress;
    std::string m_connectSessionId;
    std::unique_ptr<nx::cloud::relay::api::Client> m_relayClient;
    ConnectCompletionHandler m_handler;
    aio::Timer m_timeoutTimer;
    nx::utils::ElapsedTimer m_responseTimer;

    virtual void stopWhileInAioThread() override;

    void onStartRelaySessionResponse(
        nx::cloud::relay::api::ResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        nx::network::http::StatusCode::Value httpStatusCode,
        nx::cloud::relay::api::CreateClientSessionResponse response);
    void connectTimedOut();
};

} // namespace nx::network::cloud::relay
