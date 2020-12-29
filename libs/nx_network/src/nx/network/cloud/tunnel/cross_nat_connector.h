#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <optional>

#include <nx/network/aio/async_operation_wrapper.h>
#include <nx/network/aio/timer.h>
#include <nx/network/address_resolver.h>
#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/mediator_client_connections.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/result_counter.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/system_error.h>

#include <nx/utils/async_operation_guard.h>

#include "abstract_cross_nat_connector.h"
#include "abstract_outgoing_tunnel_connection.h"
#include "abstract_tunnel_connector.h"
#include "cloud_tunnel_connector_executor.h"
#include "connection_mediaton_initiator.h"

namespace nx {
namespace network {
namespace cloud {

class CloudConnectController;

class NX_NETWORK_API CrossNatConnector:
    public AbstractCrossNatConnector,
    public stun::UnreliableMessagePipelineEventHandler
{
public:
    /**
     * @param mediatorAddress Overrides mediator address. Introduced for testing purposes only.
     */
    CrossNatConnector(
        CloudConnectController* cloudConnectController,
        const AddressEntry& targetPeerAddress,
        std::optional<hpm::api::MediatorAddress> mediatorAddress = std::nullopt);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) override;

    virtual QString getRemotePeerName() const override;

    SocketAddress localUdpHolePunchingEndpoint() const;

    void replaceOriginatingHostAddress(const QString& address);

    static utils::ResultCounter<nx::hpm::api::ResultCode>& mediatorResponseCounter();

protected:
    virtual void stopWhileInAioThread() override;

    virtual void messageReceived(
        SocketAddress sourceAddress,
        stun::Message msg) override;

    virtual void ioFailure(SystemError::ErrorCode errorCode) override;

private:
    using MediatorUdpEndpointFetcher = aio::AsyncOperationWrapper<
        decltype(&nx::hpm::api::AbstractMediatorConnector::fetchAddress)>;

    CloudConnectController* const m_cloudConnectController;
    const AddressEntry m_targetPeerAddress;
    const std::string m_connectSessionId;
    ConnectCompletionHandler m_completionHandler;
    std::optional<hpm::api::MediatorAddress> m_mediatorAddress;
    std::unique_ptr<ConnectionMediationInitiator> m_connectionMediationInitiator;
    std::optional<QString> m_originatingHostAddressReplacement;
    SocketAddress m_localAddress;
    std::optional<std::chrono::milliseconds> m_connectTimeout;
    nx::hpm::api::ConnectionResultRequest m_connectResultReport;
    std::unique_ptr<stun::UnreliableMessagePipeline> m_connectResultReportSender;
    std::unique_ptr<AbstractOutgoingTunnelConnection> m_connection;
    bool m_done;
    nx::hpm::api::ConnectionParameters m_connectionParameters;
    QString m_remotePeerFullName;
    std::unique_ptr<aio::Timer> m_timer;
    std::unique_ptr<ConnectorExecutor> m_cloudConnectorExecutor;
    MediatorUdpEndpointFetcher m_mediatorAddressFetcher;

    static utils::ResultCounter<nx::hpm::api::ResultCode> s_mediatorResponseCounter;

    void fetchMediatorUdpEndpoint();

    void onFetchMediatorAddressCompletion(
        http::StatusCode::Value resultCode,
        hpm::api::MediatorAddress mediatorAddress);

    void issueConnectRequestToMediator();

    std::tuple<SystemError::ErrorCode, std::unique_ptr<hpm::api::MediatorClientUdpConnection>>
        prepareForUdpHolePunching();

    void onConnectResponse(
        nx::hpm::api::ResultCode resultCode,
        nx::hpm::api::ConnectResponse response);

    std::chrono::milliseconds calculateTimeLeftForConnect();

    void start(
        std::chrono::milliseconds connectTimeout,
        nx::hpm::api::ConnectResponse response);

    void onConnectorFinished(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection);

    void onTimeout();

    void holePunchingDone(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode);

    void connectSessionReportSent(SystemError::ErrorCode errorCode);

    hpm::api::ConnectRequest prepareConnectRequest(
        const SocketAddress& udpHolePunchingLocalEndpoint) const;
};

} // namespace cloud
} // namespace network
} // namespace nx
