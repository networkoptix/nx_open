#pragma once

#include <chrono>
#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include "connector_factory.h"

namespace nx {
namespace network {
namespace cloud {

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
        const nx::String& connectSessionId,
        const hpm::api::ConnectResponse& response,
        std::unique_ptr<UDPSocket> udpSocket);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void setTimeout(std::chrono::milliseconds timeout);
    void start(CompletionHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    const nx::String m_connectSessionId;
    hpm::api::ConnectResponse m_response;
    CloudConnectors m_connectors;
    std::chrono::milliseconds m_connectTimeout;
    CompletionHandler m_handler;

    void onConnectorFinished(
        CloudConnectors::iterator connectorIter,
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection);
};

} // namespace cloud
} // namespace network
} // namespace nx
