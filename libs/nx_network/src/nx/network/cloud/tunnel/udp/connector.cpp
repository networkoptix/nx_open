// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connector.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/nettools.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/format.h>
#include <nx/utils/std/cpp14.h>

#include "outgoing_tunnel_connection.h"
#include "rendezvous_connector_with_verification.h"

namespace nx::network::cloud::udp {

using namespace nx::hpm;

TunnelConnector::TunnelConnector(
    AddressEntry targetHostAddress,
    std::string connectSessionId,
    std::unique_ptr<AbstractDatagramSocket> udpSocket)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId)),
    m_udpSocket(std::move(udpSocket)),
    m_remotePeerCloudConnectVersion(hpm::api::kDefaultCloudConnectVersion)
{
    NX_CRITICAL(m_udpSocket);
    m_localAddress = m_udpSocket->getLocalAddress();

    m_timer = std::make_unique<aio::Timer>();
    m_timer->bindToAioThread(getAioThread());
}

TunnelConnector::~TunnelConnector()
{
    //it is OK if called after pleaseStop or within aio thread after connect handler has been called
    stopWhileInAioThread();
}

void TunnelConnector::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractTunnelConnector::bindToAioThread(aioThread);

    for (auto& ctx: m_rendezvousConnectors)
        ctx.connector->bindToAioThread(aioThread);
    if (m_chosenConnectorCtx)
        m_chosenConnectorCtx->connector->bindToAioThread(aioThread);
    m_timer->bindToAioThread(aioThread);
}

int TunnelConnector::getPriority() const
{
    //TODO #akolesnikov
    return 0;
}

void TunnelConnector::connect(
    const hpm::api::ConnectResponse& response,
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    m_remotePeerCloudConnectVersion = response.cloudConnectVersion;
    m_completionHandler = std::move(handler);

    //extracting target address from response
    if (response.udpEndpointList.empty())
    {
        NX_DEBUG(this, "cross-nat %1. mediator reported empty UDP address list for host %2",
            m_connectSessionId, m_targetHostAddress.host);

        return post(
            [this]()
            {
                holePunchingDone(
                    api::NatTraversalResultCode::targetPeerHasNoUdpAddress,
                    SystemError::connectionReset,
                    std::nullopt);
            });
    }

    for (const SocketAddress& endpoint: response.udpEndpointList)
    {
        auto connectorCtx = createRendezvousConnector(std::move(endpoint));

        NX_DEBUG(this, "cross-nat %1. Udt rendezvous connect to %2",
            m_connectSessionId, connectorCtx.connector->remoteAddress().toString());

        connectorCtx.responseTimer.restart();
        connectorCtx.connector->connect(
            timeout,
            [this, connectorPtr = connectorCtx.connector.get()](
                SystemError::ErrorCode errorCode)
            {
                onUdtConnectionEstablished(
                    connectorPtr,
                    errorCode);
            });
        m_rendezvousConnectors.emplace_back(std::move(connectorCtx));
    }
}

const AddressEntry& TunnelConnector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

SocketAddress TunnelConnector::localAddress() const
{
    return m_localAddress;
}

void TunnelConnector::messageReceived(
    SocketAddress /*sourceAddress*/,
    stun::Message /*msg*/)
{
    //here we can receive response to connect result report. We just don't need it
}

void TunnelConnector::ioFailure(SystemError::ErrorCode /*errorCode*/)
{
    //if error happens when sending connect result report,
    //  it will be reported to TunnelConnector::connectSessionReportSent too
    //  and we will handle error there
}

void TunnelConnector::stopWhileInAioThread()
{
    m_rendezvousConnectors.clear();
    m_chosenConnectorCtx.reset();
    m_timer.reset();
}

void TunnelConnector::onUdtConnectionEstablished(
    RendezvousConnectorWithVerification* rendezvousConnectorPtr,
    SystemError::ErrorCode errorCode)
{
    //we are in timer's aio thread
    auto connectorCtxIter = std::find_if(
        m_rendezvousConnectors.begin(),
        m_rendezvousConnectors.end(),
        [rendezvousConnectorPtr](const ConnectorContext& val)
        {
            return val.connector.get() == rendezvousConnectorPtr;
        });
    NX_CRITICAL(connectorCtxIter != m_rendezvousConnectors.end());

    connectorCtxIter->stats.responseTime = connectorCtxIter->responseTimer.elapsed();

    auto connectorCtx = std::move(*connectorCtxIter);
    m_rendezvousConnectors.erase(connectorCtxIter);

    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, "cross-nat %1. Failed to establish UDT cross-nat to %2. %3",
            m_connectSessionId,
            connectorCtx.connector->remoteAddress(),
            SystemError::toString(errorCode));

        if (!m_rendezvousConnectors.empty())
            return; //waiting for other connectors to complete

        return holePunchingDone(
            api::NatTraversalResultCode::udtConnectFailed,
            errorCode,
            std::move(connectorCtx));
    }

    //success!
    NX_VERBOSE(this, "cross-nat %1. Udp hole punching to %2 is a success!",
        m_connectSessionId, connectorCtx.connector->remoteAddress());

    //stopping other rendezvous connectors
    m_rendezvousConnectors.clear(); //can do since we are in aio thread

    m_chosenConnectorCtx = std::move(connectorCtx);

    if (m_remotePeerCloudConnectVersion <
        hpm::api::CloudConnectVersion::tryingEveryAddressOfPeer)
    {
        return onHandshakeComplete(SystemError::noError);
    }

    //notifying remote side: "this very connection has been selected"
    m_chosenConnectorCtx->connector->notifyAboutChoosingConnection(
        std::bind(&TunnelConnector::onHandshakeComplete, this, std::placeholders::_1));
}

void TunnelConnector::onHandshakeComplete(SystemError::ErrorCode errorCode)
{
    m_chosenConnectorCtx->stats.responseTime = m_chosenConnectorCtx->responseTimer.elapsed();

    auto connectorCtx = std::move(m_chosenConnectorCtx);
    m_chosenConnectorCtx.reset();

    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, "cross-nat %1. Failed to notify remote side (%2) about connection choice. %3",
            m_connectSessionId,
            connectorCtx->connector->remoteAddress(),
            SystemError::toString(errorCode));

        return holePunchingDone(
            api::NatTraversalResultCode::udtConnectFailed,
            errorCode,
            std::move(connectorCtx));
    }

    //introducing delay to give server some time to call accept (work around udt bug)
    m_timer->start(
        std::chrono::milliseconds(200),
        [this, connectorCtx = std::move(connectorCtx)]() mutable
        {
            holePunchingDone(
                api::NatTraversalResultCode::ok,
                SystemError::noError,
                std::move(connectorCtx));
        });
}

void TunnelConnector::holePunchingDone(
    api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode,
    std::optional<ConnectorContext> connectorCtx)
{
    TunnelConnectResult result {
        .resultCode = resultCode,
        .sysErrorCode = sysErrorCode,
    };
    if (connectorCtx)
        result.stats = std::move(connectorCtx->stats);

    NX_VERBOSE(this, "cross-nat %1. Udp hole punching result: %2, system result code: %3",
        m_connectSessionId, resultCode, SystemError::toString(sysErrorCode));

    //we are in aio thread
    m_timer->cancelSync();

    if (result.ok())
    {
        NX_CRITICAL(connectorCtx);
        auto udtConnection = connectorCtx->connector->takeConnection();
        NX_CRITICAL(udtConnection);

        result.connection = std::make_unique<OutgoingTunnelConnection>(
            getAioThread(),
            m_connectSessionId,
            std::move(udtConnection));
    }

    nx::utils::swapAndCall(m_completionHandler, std::move(result));
}

TunnelConnector::ConnectorContext
    TunnelConnector::createRendezvousConnector(SocketAddress endpoint)
{
    ConnectorContext ctx;
    ctx.stats.remoteAddress = endpoint.toString();
    ctx.stats.connectType = ConnectType::udpHp;

    if (m_udpSocket)
    {
        ctx.connector = std::make_unique<RendezvousConnectorWithVerification>(
            m_connectSessionId,
            std::move(endpoint),
            std::exchange(m_udpSocket, nullptr)); //< Moving system socket handler from m_mediatorUdpClient to udt connection.
        m_udpSocket.reset();
    }
    else
    {
        ctx.connector = std::make_unique<RendezvousConnectorWithVerification>(
            m_connectSessionId,
            std::move(endpoint),
            SocketAddress(HostAddress::anyHost, m_localAddress.port));
    }
    ctx.connector->bindToAioThread(getAioThread());

    return ctx;
}

} // namespace nx::network::cloud::udp
