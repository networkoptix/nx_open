#pragma once

#include <memory>

#include <nx/network/aio/timer.h>

#include "../abstract_tunnel_connector.h"
#include "api/relay_api_client.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

class NX_NETWORK_API Connector:
    public AbstractTunnelConnector
{
    using base_type = AbstractTunnelConnector;

public:
    Connector(
        nx::utils::Url relayUrl,
        AddressEntry targetHostAddress,
        nx::String connectSessionId);
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
    nx::String m_connectSessionId;
    std::unique_ptr<nx::cloud::relay::api::Client> m_relayClient;
    ConnectCompletionHandler m_handler;
    aio::Timer m_timer;

    virtual void stopWhileInAioThread() override;

    void onStartRelaySessionResponse(
        nx::cloud::relay::api::ResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        nx::cloud::relay::api::CreateClientSessionResponse response);
    void connectTimedOut();
};

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
