/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <tuple>
#include <type_traits>

#include <nx/utils/move_only_func.h>
#include <nx/utils/type_utils.h>
#include <utils/common/systemerror.h>

#include "abstract_cross_nat_connector.h"
#include "abstract_outgoing_tunnel_connection.h"
#include "abstract_tunnel_connector.h"
#include "connector_factory.h"
#include "nx/network/aio/basic_pollable.h"
#include "nx/network/cloud/address_resolver.h"
#include "nx/network/cloud/data/connect_data.h"
#include "nx/network/cloud/mediator_connections.h"

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API CrossNatConnector:
    public AbstractCrossNatConnector,
    public stun::UnreliableMessagePipelineEventHandler
{
public:
    /**
     * @param mediatorAddress Overrides mediator address. Introduced for testing purposes only.
     */
    CrossNatConnector(
        const AddressEntry& targetPeerAddress,
        boost::optional<SocketAddress> mediatorAddress = boost::none);
    virtual ~CrossNatConnector();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) override;

    SocketAddress localAddress() const;
    void replaceOriginatingHostAddress(const QString& address);

    static void setDefaultTunnelInactivityTimeout(std::chrono::seconds value);

protected:
    virtual void messageReceived(
        SocketAddress sourceAddress,
        stun::Message msg) override;
    virtual void ioFailure(SystemError::ErrorCode errorCode) override;

private:
    static std::atomic<std::chrono::seconds> s_defaultTunnelInactivityTimeout;

    const AddressEntry m_targetPeerAddress;
    const nx::String m_connectSessionId;
    ConnectorFactory::CloudConnectors m_connectors;
    ConnectCompletionHandler m_completionHandler;
    SocketAddress m_mediatorAddress;
    std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> m_mediatorUdpClient;
    boost::optional<QString> m_originatingHostAddressReplacement;
    SocketAddress m_localAddress;
    boost::optional<std::chrono::milliseconds> m_connectTimeout;
    nx::hpm::api::ConnectionResultRequest m_connectResultReport;
    std::unique_ptr<stun::UnreliableMessagePipeline> m_connectResultReportSender;
    std::unique_ptr<AbstractOutgoingTunnelConnection> m_connection;
    bool m_done;
    nx::hpm::api::ConnectionParameters m_connectionParameters;

    void issueConnectRequestToMediator(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler);
    void onConnectResponse(
        nx::hpm::api::ResultCode resultCode,
        nx::hpm::api::ConnectResponse response);
    std::chrono::milliseconds calculateTimeLeftForConnect();
    void startNatTraversing(
        std::chrono::milliseconds connectTimeout,
        nx::hpm::api::ConnectResponse response);
    std::unique_ptr<AbstractOutgoingTunnelConnection> configureConnection(
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection);
    void onConnectorFinished(
        ConnectorFactory::CloudConnectors::iterator connectorIter,
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection> connection);
    void onTimeout();
    void holePunchingDone(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode);
    void connectSessionReportSent(SystemError::ErrorCode errorCode);
    hpm::api::ConnectRequest prepareConnectRequest() const;
};

} // namespace cloud
} // namespace network
} // namespace nx
