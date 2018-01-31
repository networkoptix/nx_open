/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "connector.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/std/cpp14.h>

#include "outgoing_tunnel_connection.h"
#include "rendezvous_connector_with_verification.h"
#include "nx/network/nettools.h"

namespace nx {
namespace network {
namespace cloud {
namespace udp {

using namespace nx::hpm;

TunnelConnector::TunnelConnector(
    AddressEntry targetHostAddress,
    nx::String connectSessionId,
    std::unique_ptr<nx::network::UDPSocket> udpSocket)
:
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId)),
    m_udpSocket(std::move(udpSocket)),
    m_remotePeerCloudConnectVersion(hpm::api::kDefaultCloudConnectVersion)
{
    NX_ASSERT(nx::network::SocketGlobals::mediatorConnector().udpEndpoint());
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

    for (auto& rendezvousConnector: m_rendezvousConnectors)
        rendezvousConnector->bindToAioThread(aioThread);
    if (m_udtConnection)
        m_udtConnection->bindToAioThread(aioThread);
    if (m_chosenRendezvousConnector)
        m_chosenRendezvousConnector->bindToAioThread(aioThread);
    m_timer->bindToAioThread(aioThread);
}

int TunnelConnector::getPriority() const
{
    //TODO #ak
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
        NX_LOGX(lm("cross-nat %1. mediator reported empty UDP address list for host %2")
            .arg(m_connectSessionId).arg(m_targetHostAddress.host.toString()),
            cl_logDEBUG1);
        return post(std::bind(&TunnelConnector::holePunchingDone, this,
            api::NatTraversalResultCode::targetPeerHasNoUdpAddress,
            SystemError::connectionReset));
    }

    for (const SocketAddress& endpoint: response.udpEndpointList)
    {
        std::unique_ptr<RendezvousConnectorWithVerification> rendezvousConnector =
            createRendezvousConnector(std::move(endpoint));

        NX_LOGX(lm("cross-nat %1. Udt rendezvous connect to %2")
            .arg(m_connectSessionId).arg(rendezvousConnector->remoteAddress().toString()),
            cl_logDEBUG1);

        rendezvousConnector->connect(
            timeout,
            [this, rendezvousConnectorPtr = rendezvousConnector.get()](
                SystemError::ErrorCode errorCode)
            {
                onUdtConnectionEstablished(
                    rendezvousConnectorPtr,
                    errorCode);
            });
        m_rendezvousConnectors.emplace_back(std::move(rendezvousConnector));
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
    m_udtConnection.reset();    //we do not use this connection so can just remove it here
    m_chosenRendezvousConnector.reset();
    m_timer.reset();
}

void TunnelConnector::onUdtConnectionEstablished(
    RendezvousConnectorWithVerification* rendezvousConnectorPtr,
    SystemError::ErrorCode errorCode)
{
    //we are in timer's aio thread
    auto rendezvousConnectorIter = std::find_if(
        m_rendezvousConnectors.begin(),
        m_rendezvousConnectors.end(),
        [rendezvousConnectorPtr](
            const std::unique_ptr<RendezvousConnectorWithVerification>& val)
        {
            return val.get() == rendezvousConnectorPtr;
        });
    NX_CRITICAL(rendezvousConnectorIter != m_rendezvousConnectors.end());
    auto rendezvousConnector = std::move(*rendezvousConnectorIter);
    m_rendezvousConnectors.erase(rendezvousConnectorIter);

    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("cross-nat %1. Failed to establish UDT cross-nat to %2. %3")
            .arg(m_connectSessionId)
            .arg(rendezvousConnector->remoteAddress().toString())
            .arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);

        if (!m_rendezvousConnectors.empty())
            return; //waiting for other connectors to complete
        holePunchingDone(
            api::NatTraversalResultCode::udtConnectFailed,
            errorCode);
        return;
    }

    //success!
    NX_LOGX(lm("cross-nat %1. Udp hole punching to %2 is a success!")
        .arg(m_connectSessionId).arg(rendezvousConnector->remoteAddress().toString()),
        cl_logDEBUG2);

    //stopping other rendezvous connectors
    m_rendezvousConnectors.clear(); //can do since we are in aio thread

    m_chosenRendezvousConnector = std::move(rendezvousConnector);

    if (m_remotePeerCloudConnectVersion <
        hpm::api::CloudConnectVersion::tryingEveryAddressOfPeer)
    {
        return onHandshakeComplete(SystemError::noError);
    }

    //notifying remote side: "this very connection has been selected"

    m_chosenRendezvousConnector->notifyAboutChoosingConnection(
        std::bind(&TunnelConnector::onHandshakeComplete, this, std::placeholders::_1));
}

void TunnelConnector::onHandshakeComplete(SystemError::ErrorCode errorCode)
{
    auto rendezvousConnector = std::move(m_chosenRendezvousConnector);
    m_chosenRendezvousConnector.reset();

    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("cross-nat %1. Failed to notify remote side (%2) about connection choice. %3")
            .arg(m_connectSessionId)
            .arg(rendezvousConnector->remoteAddress().toString())
            .arg(SystemError::toString(errorCode)),
            cl_logDEBUG1);

        holePunchingDone(
            api::NatTraversalResultCode::udtConnectFailed,
            errorCode);
        return;
    }

    m_udtConnection = rendezvousConnector->takeConnection();
    NX_CRITICAL(m_udtConnection);
    rendezvousConnector.reset();

    //introducing delay to give server some time to call accept (work around udt bug)
    m_timer->start(
        std::chrono::milliseconds(200),
        [this]
        {
            holePunchingDone(
                api::NatTraversalResultCode::ok,
                SystemError::noError);
        });
}

void TunnelConnector::holePunchingDone(
    api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode)
{
    NX_LOGX(lm("cross-nat %1. Udp hole punching result: %2, system result code: %3")
        .arg(m_connectSessionId).arg(QnLexical::serialized(resultCode))
        .arg(SystemError::toString(sysErrorCode)),
        cl_logDEBUG2);

    //we are in aio thread
    m_timer->cancelSync();

    std::unique_ptr<AbstractOutgoingTunnelConnection> tunnelConnection;
    if (resultCode == api::NatTraversalResultCode::ok)
    {
        tunnelConnection = std::make_unique<OutgoingTunnelConnection>(
            getAioThread(),
            m_connectSessionId,
            std::move(m_udtConnection));
    }

    nx::utils::swapAndCall(
        m_completionHandler,
        resultCode,
        sysErrorCode,
        std::move(tunnelConnection));
}

std::unique_ptr<RendezvousConnectorWithVerification>
    TunnelConnector::createRendezvousConnector(SocketAddress endpoint)
{
    std::unique_ptr<RendezvousConnectorWithVerification> rendezvousConnector;
    if (m_udpSocket)
    {
        rendezvousConnector = std::make_unique<RendezvousConnectorWithVerification>(
            m_connectSessionId,
            std::move(endpoint),
            std::move(m_udpSocket));  //moving system socket handler from m_mediatorUdpClient to udt connection
        m_udpSocket.reset();
    }
    else
    {
        rendezvousConnector = std::make_unique<RendezvousConnectorWithVerification>(
            m_connectSessionId,
            std::move(endpoint),
            SocketAddress(HostAddress::anyHost, m_localAddress.port));
    }
    rendezvousConnector->bindToAioThread(getAioThread());

    return rendezvousConnector;
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
