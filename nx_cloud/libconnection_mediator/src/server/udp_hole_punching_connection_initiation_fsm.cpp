/**********************************************************
* Jan 15, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connection_initiation_fsm.h"

#include <chrono>

#include <nx/network/cloud/data/connection_ack_data.h>
#include <nx/network/cloud/data/connection_requested_event_data.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include "settings.h"

namespace nx {
namespace hpm {

UDPHolePunchingConnectionInitiationFsm::UDPHolePunchingConnectionInitiationFsm(
    nx::String connectionID,
    const ListeningPeerData& serverPeerData,
    std::function<void(api::NatTraversalResultCode)> onFsmFinishedEventHandler,
    const conf::Settings& settings)
:
    m_state(State::init),
    m_connectionID(std::move(connectionID)),
    m_onFsmFinishedEventHandler(std::move(onFsmFinishedEventHandler)),
    m_settings(settings),
    m_serverConnectionWeakRef(serverPeerData.peerConnection),
    m_directTcpAddresses(serverPeerData.endpoints)
{
    auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
    if (!serverConnectionStrongRef)
    {
        post(std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::serverConnectionBroken));
        return;
    }

    m_sessionStatisticsInfo.startTime = nx::utils::utcTime();
    m_sessionStatisticsInfo.sessionId = connectionID;

    bindToAioThread(serverConnectionStrongRef->socket()->getAioThread());
}

UDPHolePunchingConnectionInitiationFsm::~UDPHolePunchingConnectionInitiationFsm()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void UDPHolePunchingConnectionInitiationFsm::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    Parent::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
}

void UDPHolePunchingConnectionInitiationFsm::onConnectRequest(
    const ConnectionStrongRef& originatingPeerConnection,
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> connectResponseSender)
{
    dispatch(
        [this, originatingPeerConnection,
            request = std::move(request), connectResponseSender]()
        {
            processConnectRequest(
                originatingPeerConnection,
                std::move(request),
                std::move(connectResponseSender));
        });
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionAckRequest(
    const ConnectionStrongRef& connection,
    api::ConnectionAckRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    dispatch(
        [this, connection, request = std::move(request),
            completionHandler = std::move(completionHandler)]() mutable
        {
            processConnectionAckRequest(
                connection,
                std::move(request),
                std::move(completionHandler));
        });
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionResultRequest(
    api::ConnectionResultRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    dispatch(
        [this, request, completionHandler = std::move(completionHandler)]()
        {
            processConnectionResultRequest(
                std::move(request), std::move(completionHandler));
        });
}

stats::ConnectSession UDPHolePunchingConnectionInitiationFsm::statisticsInfo() const
{
    return m_sessionStatisticsInfo;
}

void UDPHolePunchingConnectionInitiationFsm::stopWhileInAioThread()
{
    m_timer.pleaseStopSync();
}

void UDPHolePunchingConnectionInitiationFsm::processConnectRequest(
    const ConnectionStrongRef& originatingPeerConnection,
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> connectResponseSender)
{
    if (connectResponseHasAlreadyBeenSent())
        return repeatConnectResponse(std::move(connectResponseSender));

    updateSessionStatistics(originatingPeerConnection, request);

    NX_ASSERT(connectResponseSender);
    m_connectResponseSender = std::move(connectResponseSender);

    if (!notifyListeningPeerAboutConnectRequest(originatingPeerConnection, request))
        return;

    m_timer.start(
        m_settings.connectionParameters().connectionAckAwaitTimeout,
        std::bind(&UDPHolePunchingConnectionInitiationFsm::noConnectionAckOnTime, this));
    m_state = State::waitingServerPeerUDPAddress;
}

bool UDPHolePunchingConnectionInitiationFsm::connectResponseHasAlreadyBeenSent() const
{
    if (m_cachedConnectResponse)
    {
        NX_ASSERT(m_state >= State::waitingConnectionResult);
        return true;
    }
    return false;
}

void UDPHolePunchingConnectionInitiationFsm::repeatConnectResponse(
    std::function<void(api::ResultCode, api::ConnectResponse)> connectResponseSender)
{
    connectResponseSender(m_cachedConnectResponse->first, m_cachedConnectResponse->second);
}

void UDPHolePunchingConnectionInitiationFsm::updateSessionStatistics(
    const ConnectionStrongRef& originatingPeerConnection,
    const api::ConnectRequest& connectRequest)
{
    m_sessionStatisticsInfo.originatingHostEndpoint =
        originatingPeerConnection->getSourceAddress().toString().toUtf8();
    m_sessionStatisticsInfo.originatingHostName = connectRequest.originatingPeerId;
    m_sessionStatisticsInfo.destinationHostName = connectRequest.destinationHostName;
}

bool UDPHolePunchingConnectionInitiationFsm::notifyListeningPeerAboutConnectRequest(
    const ConnectionStrongRef& originatingPeerConnection,
    const api::ConnectRequest& connectRequest)
{
    auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
    if (!serverConnectionStrongRef)
    {
        post(std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::serverConnectionBroken));
        return false;
    }
    auto indication = prepareConnectionRequestedIndication(
        originatingPeerConnection,
        connectRequest);
    serverConnectionStrongRef->sendMessage(std::move(indication));  //< TODO #ak: Check sendMessage result.

    return true;
}

nx::stun::Message UDPHolePunchingConnectionInitiationFsm::prepareConnectionRequestedIndication(
    const ConnectionStrongRef& originatingPeerConnection,
    const api::ConnectRequest& connectRequest)
{
    api::ConnectionRequestedEvent connectionRequestedEvent;
    connectionRequestedEvent.connectSessionId = std::move(connectRequest.connectSessionId);
    connectionRequestedEvent.originatingPeerID = std::move(connectRequest.originatingPeerId);
    std::move(
        connectRequest.udpEndpointList.begin(),
        connectRequest.udpEndpointList.end(),
        std::back_inserter(connectionRequestedEvent.udpEndpointList));
    const auto originatingPeerSourceAddress = originatingPeerConnection->getSourceAddress();
    //setting ports if needed
    for (auto& endpoint: connectionRequestedEvent.udpEndpointList)
    {
        if (endpoint.port == 0)
            endpoint.port = originatingPeerSourceAddress.port;
    }
    if (!connectRequest.ignoreSourceAddress)
    {
        connectionRequestedEvent.udpEndpointList.emplace_front(
            originatingPeerSourceAddress);
    }
    connectionRequestedEvent.connectionMethods =
        api::ConnectionMethod::udpHolePunching;
    connectionRequestedEvent.params = m_settings.connectionParameters();
    nx::stun::Message indication(
        stun::Header(
            stun::MessageClass::indication,
            stun::extension::indications::connectionRequested));
    connectionRequestedEvent.serialize(&indication);
    connectionRequestedEvent.cloudConnectVersion = connectRequest.cloudConnectVersion;

    return indication;
}

void UDPHolePunchingConnectionInitiationFsm::noConnectionAckOnTime()
{
    // Sending connect response.
    m_state = State::waitingConnectionResult;

    api::ConnectResponse connectResponse = prepareConnectResponse(
        api::ConnectionAckRequest(), std::list<SocketAddress>());
    sendConnectResponse(api::ResultCode::noReplyFromServer, std::move(connectResponse));

    if (m_settings.connectionParameters().connectionResultWaitTimeout == 
        std::chrono::seconds::zero())
    {
        return done(api::ResultCode::timedOut);
    }

    // Waiting for connection result report.
    m_timer.start(
        m_settings.connectionParameters().connectionResultWaitTimeout,
        std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::timedOut));
}

void UDPHolePunchingConnectionInitiationFsm::processConnectionAckRequest(
    const ConnectionStrongRef& connection,
    api::ConnectionAckRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_sessionStatisticsInfo.destinationHostEndpoint =
        connection->getSourceAddress().toString().toUtf8();

    if (m_state > State::waitingServerPeerUDPAddress)
    {
        NX_LOGX(lm("Connection %1. Received connectionAck while in %2 state. Ignoring...")
            .arg(m_connectionID).arg(toString(m_state)), cl_logDEBUG1);
        completionHandler(api::ResultCode::ok);
        return;
    }

    if (connection->transportProtocol() == nx::network::TransportProtocol::udp)
        request.udpEndpointList.push_front(connection->getSourceAddress());

    if (request.udpEndpointList.empty() && request.forwardedTcpEndpointList.empty())
    {
        completionHandler(api::ResultCode::noSuitableConnectionMethod);
        post(std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::noSuitableConnectionMethod));
        return;
    }

    auto tcpEndpoints = std::move(request.forwardedTcpEndpointList);
    tcpEndpoints.insert(
        tcpEndpoints.begin(), m_directTcpAddresses.begin(), m_directTcpAddresses.end());

    m_state = State::waitingConnectionResult;

    api::ConnectResponse connectResponse = prepareConnectResponse(
        request, std::move(tcpEndpoints));
    sendConnectResponse(api::ResultCode::ok, std::move(connectResponse));

    completionHandler(api::ResultCode::ok);

    m_timer.start(
        m_settings.connectionParameters().connectionResultWaitTimeout,
        std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::timedOut));
}

api::ConnectResponse UDPHolePunchingConnectionInitiationFsm::prepareConnectResponse(
    const api::ConnectionAckRequest& connectionAckRequest,
    std::list<SocketAddress> tcpEndpoints)
{
    api::ConnectResponse connectResponse;
    connectResponse.params = m_settings.connectionParameters();
    connectResponse.udpEndpointList = std::move(connectionAckRequest.udpEndpointList);
    connectResponse.forwardedTcpEndpointList = std::move(tcpEndpoints);
    connectResponse.cloudConnectVersion = connectionAckRequest.cloudConnectVersion;

    return connectResponse;
}

void UDPHolePunchingConnectionInitiationFsm::sendConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse connectResponse)
{
    NX_CRITICAL(m_connectResponseSender);
    decltype(m_connectResponseSender) connectResponseSender;
    connectResponseSender.swap(m_connectResponseSender);

    m_cachedConnectResponse = std::make_pair(resultCode, connectResponse);

    connectResponseSender(resultCode, std::move(connectResponse));
}

void UDPHolePunchingConnectionInitiationFsm::processConnectionResultRequest(
    api::ConnectionResultRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (m_state < State::waitingConnectionResult)
    {
        NX_LOGX(lm("Connection %1. Received ConnectionResultRequest while in state %2")
            .arg(m_connectionID).arg(toString(m_state)), cl_logDEBUG2);
    }

    m_sessionStatisticsInfo.resultCode = request.resultCode;

    completionHandler(api::ResultCode::ok);
    post(std::bind(
        &UDPHolePunchingConnectionInitiationFsm::done,
        this,
        api::ResultCode::ok));
}

void UDPHolePunchingConnectionInitiationFsm::done(api::ResultCode result)
{
    if (m_state < State::waitingConnectionResult)
        sendConnectResponse(result, api::ConnectResponse());

    m_state = State::fini;

    auto onFinishedHandler = std::move(m_onFsmFinishedEventHandler);
    onFinishedHandler(m_sessionStatisticsInfo.resultCode);
}

const char* UDPHolePunchingConnectionInitiationFsm::toString(State state)
{
    switch (state)
    {
        case State::init:
            return "init";
        case State::waitingServerPeerUDPAddress:
            return "waitingServerPeerUDPAddress";
        case State::waitingConnectionResult:
            return "waitingConnectionResult";
        case State::fini:
            return "fini";
        default:
            return "unknown";
    }
}

} // namespace hpm
} // namespace nx
