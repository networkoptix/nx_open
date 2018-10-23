#include "udp_hole_punching_connection_initiation_fsm.h"

#include <chrono>

#include <nx/network/cloud/data/connection_ack_data.h>
#include <nx/network/cloud/data/connection_requested_event_data.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include "settings.h"

namespace nx {
namespace hpm {

using namespace nx::network;

UDPHolePunchingConnectionInitiationFsm::UDPHolePunchingConnectionInitiationFsm(
    nx::String connectionID,
    const ListeningPeerData& serverPeerData,
    std::function<void(api::NatTraversalResultCode)> onFsmFinishedEventHandler,
    const conf::Settings& settings,
    AbstractRelayClusterClient* const relayClusterClient)
:
    m_state(State::init),
    m_connectionID(std::move(connectionID)),
    m_onFsmFinishedEventHandler(std::move(onFsmFinishedEventHandler)),
    m_settings(settings),
    m_relayClusterClient(relayClusterClient),
    m_serverConnectionWeakRef(serverPeerData.peerConnection),
    m_serverPeerEndpoints(serverPeerData.endpoints),
    m_serverPeerHostName(serverPeerData.hostName),
    m_serverPeerCloudConnectVersion(serverPeerData.cloudConnectVersion),
    m_serverPeerConnectionMethods(api::ConnectionMethod::all),
    m_originatingPeerCloudConnectVersion(api::CloudConnectVersion::initial),
    m_findRelayInstanceFunc(
        &AbstractRelayClusterClient::findRelayInstancePeerIsListeningOn)
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
    m_sessionStatisticsInfo.destinationHostEndpoint =
        serverConnectionStrongRef->socket()->getForeignAddress().toString().toUtf8();

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
    base_type::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
    m_findRelayInstanceFunc.bindToAioThread(aioThread);
}

void UDPHolePunchingConnectionInitiationFsm::onConnectRequest(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectRequest request,
    ConnectCompletionHandler connectResponseSender)
{
    dispatch(
        [this, requestSourceDescriptor,
            request = std::move(request), connectResponseSender]()
        {
            processConnectRequest(
                requestSourceDescriptor,
                std::move(request),
                std::move(connectResponseSender));
        });
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionAckRequest(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectionAckRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    dispatch(
        [this, requestSourceDescriptor, request = std::move(request),
            completionHandler = std::move(completionHandler)]() mutable
        {
            processConnectionAckRequest(
                requestSourceDescriptor,
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
    m_findRelayInstanceFunc.pleaseStopSync();
    m_asyncOperationGuard->terminate();
}

void UDPHolePunchingConnectionInitiationFsm::processConnectRequest(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectRequest request,
    ConnectCompletionHandler connectResponseSender)
{
    switch (requestSourceDescriptor.transportProtocol)
    {
        case network::TransportProtocol::udp:
            processUdpConnectRequest(
                requestSourceDescriptor,
                std::move(request),
                std::move(connectResponseSender));
            break;

        default:
            processTcpConnectRequest(
                requestSourceDescriptor,
                std::move(request),
                std::move(connectResponseSender));
            break;
    }
}

void UDPHolePunchingConnectionInitiationFsm::processUdpConnectRequest(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectRequest request,
    ConnectCompletionHandler connectResponseSender)
{
    if (connectResponseHasAlreadyBeenSent())
        return repeatConnectResponse(std::move(connectResponseSender));

    updateSessionStatistics(requestSourceDescriptor, request);

    NX_ASSERT(connectResponseSender);
    m_connectResponseSender = std::move(connectResponseSender);

    m_originatingPeerCloudConnectVersion = request.cloudConnectVersion;

    if (!notifyListeningPeerAboutConnectRequest(requestSourceDescriptor, request))
        return;

    m_timer.start(
        m_settings.connectionParameters().connectionAckAwaitTimeout,
        [this]() { noConnectionAckOnTime(); });
    m_state = State::waitingServerPeerUDPAddress;
}

void UDPHolePunchingConnectionInitiationFsm::processTcpConnectRequest(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectRequest request,
    ConnectCompletionHandler connectResponseSender)
{
    updateSessionStatistics(requestSourceDescriptor, request);

    findRelayInstance(
        [this, connectResponseSender = std::move(connectResponseSender)](
            nx::cloud::relay::api::ResultCode resultCode,
            QUrl relayInstanceUrl)
        {
            auto connectResponse = prepareConnectResponse(
                api::ConnectionAckRequest(),
                m_serverPeerEndpoints,
                resultCode == nx::cloud::relay::api::ResultCode::ok
                    ? std::make_optional(relayInstanceUrl)
                    : std::nullopt);

            connectResponseSender(
                api::ResultCode::ok,
                std::move(connectResponse));
        });
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
    ConnectCompletionHandler connectResponseSender)
{
    connectResponseSender(
        m_cachedConnectResponse->first,
        m_cachedConnectResponse->second);
}

void UDPHolePunchingConnectionInitiationFsm::updateSessionStatistics(
    const RequestSourceDescriptor& requestSourceDescriptor,
    const api::ConnectRequest& connectRequest)
{
    m_sessionStatisticsInfo.originatingHostEndpoint =
        requestSourceDescriptor.sourceAddress.toString().toUtf8();
    m_sessionStatisticsInfo.originatingHostName = connectRequest.originatingPeerId;
    m_sessionStatisticsInfo.destinationHostName = connectRequest.destinationHostName;
}

bool UDPHolePunchingConnectionInitiationFsm::notifyListeningPeerAboutConnectRequest(
    const RequestSourceDescriptor& requestSourceDescriptor,
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
        requestSourceDescriptor,
        connectRequest);
    serverConnectionStrongRef->sendMessage(std::move(indication));  //< TODO #ak: Check sendMessage result.

    return true;
}

nx::network::stun::Message
    UDPHolePunchingConnectionInitiationFsm::prepareConnectionRequestedIndication(
        const RequestSourceDescriptor& requestSourceDescriptor,
        const api::ConnectRequest& connectRequest)
{
    api::ConnectionRequestedEvent connectionRequestedEvent;
    connectionRequestedEvent.connectSessionId = std::move(connectRequest.connectSessionId);
    connectionRequestedEvent.originatingPeerID = std::move(connectRequest.originatingPeerId);
    std::move(
        connectRequest.udpEndpointList.begin(),
        connectRequest.udpEndpointList.end(),
        std::back_inserter(connectionRequestedEvent.udpEndpointList));

    if (requestSourceDescriptor.transportProtocol == nx::network::TransportProtocol::udp)
    {
        const auto originatingPeerSourceAddress = requestSourceDescriptor.sourceAddress;
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
    }

    connectionRequestedEvent.connectionMethods =
        api::ConnectionMethod::udpHolePunching;
    connectionRequestedEvent.params = m_settings.connectionParameters();
    nx::network::stun::Message indication(
        stun::Header(
            stun::MessageClass::indication,
            stun::extension::indications::connectionRequested));
    connectionRequestedEvent.serialize(&indication);
    connectionRequestedEvent.cloudConnectVersion = connectRequest.cloudConnectVersion;

    return indication;
}

void UDPHolePunchingConnectionInitiationFsm::noConnectionAckOnTime()
{
    NX_VERBOSE(this, lm("Connection %1. No connection ack has been reported on time")
        .args(m_connectionID));

    m_timer.pleaseStopSync();

    if (initiateCloudConnect(api::ConnectionAckRequest()))
    {
        NX_VERBOSE(this, "Proceeding without connection ack from listening peer");
        return;
    }

    NX_VERBOSE(this, "Reporting connect failure");

    // Sending connect response.
    m_state = State::waitingConnectionResult;

    api::ConnectResponse connectResponse = prepareConnectResponse(
        api::ConnectionAckRequest(),
        std::list<network::SocketAddress>(),
        std::nullopt);
    sendConnectResponse(api::ResultCode::noReplyFromServer, std::move(connectResponse));

    if (m_settings.connectionParameters().connectionResultWaitTimeout ==
        std::chrono::seconds::zero())
    {
        return done(api::ResultCode::timedOut);
    }

    // Waiting for connection result report.
    m_timer.start(
        m_settings.connectionParameters().connectionResultWaitTimeout,
        [this]() { done(api::ResultCode::timedOut); });
}

void UDPHolePunchingConnectionInitiationFsm::processConnectionAckRequest(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectionAckRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_sessionStatisticsInfo.destinationHostEndpoint =
        requestSourceDescriptor.sourceAddress.toString().toUtf8();

    if (requestSourceDescriptor.transportProtocol == nx::network::TransportProtocol::udp)
        request.udpEndpointList.push_front(requestSourceDescriptor.sourceAddress);

    m_timer.pleaseStopSync();

    if (m_state > State::waitingServerPeerUDPAddress)
    {
        NX_DEBUG(this, lm("Connection %1. Received connectionAck while in %2 state. Ignoring...")
            .args(m_connectionID, toString(m_state)));
        completionHandler(api::ResultCode::ok);
        return;
    }

    m_serverPeerConnectionMethods = request.connectionMethods;
    if (!initiateCloudConnect(std::move(request)))
        return completionHandler(api::ResultCode::noSuitableConnectionMethod);

    // Saving completion handler so that client and server receive
    // connect and connectionAck responses simultaneously.
    m_connectionAckCompletionHandler = std::move(completionHandler);
}

bool UDPHolePunchingConnectionInitiationFsm::initiateCloudConnect(
    api::ConnectionAckRequest connectionAck)
{
    decltype(connectionAck.forwardedTcpEndpointList) tcpEndpoints;
    tcpEndpoints.swap(connectionAck.forwardedTcpEndpointList);
    tcpEndpoints.insert(
        tcpEndpoints.begin(),
        m_serverPeerEndpoints.begin(), m_serverPeerEndpoints.end());

    if (connectionAck.udpEndpointList.empty() &&
        tcpEndpoints.empty() &&
        (m_serverPeerConnectionMethods & api::ConnectionMethod::proxy) == 0)
    {
        post([this]() { done(api::ResultCode::noSuitableConnectionMethod); });
        return false;
    }

    m_preparedConnectResponse = prepareConnectResponse(
        connectionAck,
        std::move(tcpEndpoints),
        std::nullopt);

    initiateRelayInstanceSearch();
    return true;
}

void UDPHolePunchingConnectionInitiationFsm::initiateRelayInstanceSearch()
{
    m_state = State::resolvingServersRelayInstance;

    auto completionHander = 
        [this](
            nx::cloud::relay::api::ResultCode resultCode,
            QUrl relayInstanceUrl)
        {
            onRelayInstanceSearchCompletion(
                resultCode == nx::cloud::relay::api::ResultCode::ok
                    ? std::make_optional(std::move(relayInstanceUrl))
                    : std::nullopt);
        };

    findRelayInstance(std::move(completionHander));
}

void UDPHolePunchingConnectionInitiationFsm::findRelayInstance(
    RelayInstanceSearchCompletionHandler handler)
{
    auto sharedHandler = 
        std::make_shared<RelayInstanceSearchCompletionHandler>(std::move(handler));

    m_findRelayInstanceFunc.invokeWithTimeout(
        [sharedHandler](auto... args)
        {
            nx::utils::swapAndCall(
                *sharedHandler,
                std::move(args)...);
        },
        m_settings.connectionParameters().maxRelayInstanceSearchTime,
        [sharedHandler]()
        {
            nx::utils::swapAndCall(
                *sharedHandler,
                nx::cloud::relay::api::ResultCode::timedOut,
                QUrl());
        },
        m_relayClusterClient,
        m_serverPeerHostName.toStdString());
}

void UDPHolePunchingConnectionInitiationFsm::finishConnect()
{
    api::ResultCode resultCode = api::ResultCode::ok;
    if (m_preparedConnectResponse.udpEndpointList.empty()
        && m_preparedConnectResponse.forwardedTcpEndpointList.empty()
        && !static_cast<bool>(m_preparedConnectResponse.trafficRelayUrl))
    {
        resultCode = api::ResultCode::noSuitableConnectionMethod;
    }

    sendConnectResponse(resultCode, std::move(m_preparedConnectResponse));
    m_state = State::waitingConnectionResult;

    if (m_connectionAckCompletionHandler)
        nx::utils::swapAndCall(m_connectionAckCompletionHandler, api::ResultCode::ok);

    m_timer.start(
        m_settings.connectionParameters().connectionResultWaitTimeout,
        std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::timedOut));
}

void UDPHolePunchingConnectionInitiationFsm::onRelayInstanceSearchCompletion(
    std::optional<QUrl> relayInstanceUrl)
{
    if (relayInstanceUrl)
    {
        NX_VERBOSE(this, lm("Connection %1. Found relay instance %2")
            .args(m_connectionID, *relayInstanceUrl));
        m_preparedConnectResponse.trafficRelayUrl = relayInstanceUrl->toString().toUtf8();
    }
    else
    {
        NX_WARNING(this, lm("Connection %1. Could not find a relay instance")
            .args(m_connectionID));
    }

    if (m_state > State::resolvingServersRelayInstance)
    {
        NX_DEBUG(this, lm("Ignoring late \"find relay instance\" response"));
        return;
    }

    m_timer.pleaseStopSync();

    finishConnect();
}

api::ConnectResponse UDPHolePunchingConnectionInitiationFsm::prepareConnectResponse(
    const api::ConnectionAckRequest& connectionAckRequest,
    std::list<network::SocketAddress> tcpEndpoints,
    std::optional<QUrl> relayInstanceUrl)
{
    api::ConnectResponse connectResponse;
    connectResponse.params = m_settings.connectionParameters();
    connectResponse.udpEndpointList = connectionAckRequest.udpEndpointList;
    connectResponse.forwardedTcpEndpointList = std::move(tcpEndpoints);
    connectResponse.cloudConnectVersion = m_serverPeerCloudConnectVersion;
    connectResponse.destinationHostFullName = m_serverPeerHostName;
    if (relayInstanceUrl)
        connectResponse.trafficRelayUrl = relayInstanceUrl->toString().toUtf8();

    return connectResponse;
}

void UDPHolePunchingConnectionInitiationFsm::sendConnectResponse(
    api::ResultCode resultCode,
    api::ConnectResponse connectResponse)
{
    NX_VERBOSE(this, lm("Connection %1. Sending connect response (%2) while in state %3")
        .args(m_connectionID, QnLexical::serialized(resultCode), toString(m_state)));

    NX_CRITICAL(m_connectResponseSender);
    decltype(m_connectResponseSender) connectResponseSender;
    connectResponseSender.swap(m_connectResponseSender);

    fixConnectResponseForBuggyClient(resultCode, &connectResponse);

    m_cachedConnectResponse = std::make_pair(resultCode, connectResponse);
    connectResponseSender(resultCode, std::move(connectResponse));
}

void UDPHolePunchingConnectionInitiationFsm::fixConnectResponseForBuggyClient(
    api::ResultCode resultCode,
    api::ConnectResponse* const connectResponse)
{
    if (resultCode != api::ResultCode::ok)
        return;

    if (m_originatingPeerCloudConnectVersion <
            api::CloudConnectVersion::clientSupportsConnectSessionWithoutUdpEndpoints &&
        connectResponse->udpEndpointList.empty())
    {
        // Reporting dummy UDP endpoint to the buggy client.
        connectResponse->udpEndpointList.push_back(SocketAddress("1.2.3.4:12345"));
    }
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
