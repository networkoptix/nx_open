#include "udp_hole_punching_connection_initiation_fsm.h"

#include <chrono>

#include <nx/fusion/model_functions.h>
#include <nx/network/cloud/data/connection_ack_data.h>
#include <nx/network/cloud/data/connection_requested_event_data.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>

#include "../settings.h"

namespace nx {
namespace hpm {

using namespace nx::network;

UDPHolePunchingConnectionInitiationFsm::UDPHolePunchingConnectionInitiationFsm(
    nx::String connectionId,
    const ListeningPeerData& serverPeerData,
    std::function<void(api::NatTraversalResultCode)> onFsmFinishedEventHandler,
    const conf::Settings& settings,
    AbstractRelayClusterClient* const relayClusterClient)
:
    m_state(State::init),
    m_connectionId(std::move(connectionId)),
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
        &AbstractRelayClusterClient::findRelayInstanceForClient)
{
    m_sessionStatisticsInfo.startTime = nx::utils::utcTime();
    m_sessionStatisticsInfo.sessionId = connectionId;

    if (auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock())
    {
        bindToAioThread(serverConnectionStrongRef->socket()->getAioThread());
        m_sessionStatisticsInfo.destinationHostEndpoint =
            serverConnectionStrongRef->socket()->getForeignAddress().toString().toUtf8();
    }
    else
    {
        bindToAioThread(getAioThread());
    }
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
            connectRequestReceived(
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
            connectionAckReceived(
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
            connectionResultReceived(
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

//---------------------------------------------------------------------------------------------
// FSM implementation.

bool UDPHolePunchingConnectionInitiationFsm::changeState(Event event)
{
    const auto newState = getDestinationState(m_state, event);
    if (!newState)
    {
        NX_VERBOSE(this, "Connection %1. No transition from state %2 on event %3",
            m_connectionId, m_state, event);
        return false;
    }

    NX_VERBOSE(this, "Connection %1. Moving from state %2 to %3 on event %4",
        m_connectionId, m_state, *newState, event);

    invokeOnExitingStateHandler(m_state);

    m_state = *newState;

    invokeOnEnteredStateHandler(m_state);

    return true;
}

std::optional<State> UDPHolePunchingConnectionInitiationFsm::getDestinationState(
    State from, Event event) const
{
    switch (from)
    {
        case State::init:
            if (event == Event::connectOverUdpRequestReceived)
                return State::sendingConnectionRequestedIndication;
            else if (event == Event::connectOverTcpRequestReceived)
                return State::resolvingServersRelayInstance;
            break;

        case State::sendingConnectionRequestedIndication:
            if (event == Event::connectionRequestedSent)
                return State::waitingServerPeerUdpAddress;
            else if (event == Event::connectionRequestedSendFailed)
                return State::waitingConnectionResult;
            break;

        case State::waitingServerPeerUdpAddress:
            if (event == Event::connectionAckReceived)
                return State::resolvingServersRelayInstance;
            else if (event == Event::connectionAckWaitTimedOut)
                return State::resolvingServersRelayInstance;
            break;

        case State::resolvingServersRelayInstance:
            if (event == Event::relayResolveCompleted)
                return State::waitingConnectionResult;
            break;

        case State::waitingConnectionResult:
            if (event == Event::connectOverUdpRequestReceived)
                return State::waitingConnectionResult;
            else if (event == Event::connectOverTcpRequestReceived)
                return State::waitingConnectionResult;
            else if (event == Event::connectionResultReceived)
                return State::fini;
            else if (event == Event::connectionResultWaitTimedOut)
                return State::fini;
            break;

        case State::fini:
            break;
    }

    return std::nullopt;
}

void UDPHolePunchingConnectionInitiationFsm::invokeOnExitingStateHandler(State state)
{
    NX_VERBOSE(this, "Connection %1. Invoking %2 state \"exiting\" handler", m_connectionId);

    switch (state)
    {
        case State::waitingServerPeerUdpAddress:
            stopNoConnectionAckTimer();
            break;

        case State::waitingConnectionResult:
            stopConnectionResultWaitTimer();
            break;

        default:
            break;
    }
}

void UDPHolePunchingConnectionInitiationFsm::invokeOnEnteredStateHandler(State state)
{
    NX_VERBOSE(this, "Connection %1. Invoking %2 state \"entered\" handler", m_connectionId);

    switch (state)
    {
        case State::sendingConnectionRequestedIndication:
            sendConnectionRequestedNotification();
            break;

        case State::waitingServerPeerUdpAddress:
            startNoConnectionAckTimer();
            break;

        case State::resolvingServersRelayInstance:
            initiateRelayInstanceSearch();
            break;

        case State::waitingConnectionResult:
            sendConnectResponse();
            sendConnectionAckResponse();
            startConnectionResultWaitTimer();
            exitFsmIfReportHasAlreadyBeenReceived();
            break;

        case State::fini:
            reportFsmCompletion();
            break;

        default:
            // No handler installed for the state.
            break;
    }
}

//---------------------------------------------------------------------------------------------
// Events.

void UDPHolePunchingConnectionInitiationFsm::connectRequestReceived(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectRequest request,
    ConnectCompletionHandler connectResponseSender)
{
    NX_ASSERT(isInSelfAioThread());

    if (requestSourceDescriptor.transportProtocol != network::TransportProtocol::udp &&
        requestSourceDescriptor.transportProtocol != network::TransportProtocol::tcp)
    {
        // TODO: #ak Move to some state. Otherwise, the object will hang.
        // But, reaching this code means a bug somewhere.
        NX_ASSERT(false, "Connection %1. Enexpected transport protocol %1",
            m_connectionId, (int) requestSourceDescriptor.transportProtocol);
        return connectResponseSender(api::ResultCode::badRequest, api::ConnectResponse());
    }

    m_clientEndpoint = requestSourceDescriptor;
    m_connectRequest = std::move(request);
    m_connectResponseSenders.push_back({
        requestSourceDescriptor.transportProtocol,
        std::move(connectResponseSender)});
    m_originatingPeerCloudConnectVersion = request.cloudConnectVersion;

    updateSessionStatistics(requestSourceDescriptor, request);

    changeState(
        requestSourceDescriptor.transportProtocol == network::TransportProtocol::udp
        ? Event::connectOverUdpRequestReceived
        : Event::connectOverTcpRequestReceived);
}

void UDPHolePunchingConnectionInitiationFsm::connectionRequestedNotificationSent()
{
    NX_ASSERT(isInSelfAioThread());

    changeState(Event::connectionRequestedSent);
}

void UDPHolePunchingConnectionInitiationFsm::connectionRequestedNotificationFailed()
{
    NX_ASSERT(isInSelfAioThread());

    changeState(Event::connectionRequestedSendFailed);
}

void UDPHolePunchingConnectionInitiationFsm::connectionAckReceived(
    const RequestSourceDescriptor& requestSourceDescriptor,
    api::ConnectionAckRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    NX_ASSERT(isInSelfAioThread());

    m_sessionStatisticsInfo.destinationHostEndpoint =
        requestSourceDescriptor.sourceAddress.toString().toUtf8();

    if (requestSourceDescriptor.transportProtocol == nx::network::TransportProtocol::udp)
    {
        request.udpEndpointList.insert(
            request.udpEndpointList.begin(), requestSourceDescriptor.sourceAddress);
    }

    m_connectionAckRequest = std::move(request);
    m_serverPeerConnectionMethods = request.connectionMethods;

    // Saving completion handler so that client and server receive
    // connect and connectionAck responses simultaneously.
    m_connectionAckCompletionHandler = std::move(completionHandler);

    changeState(Event::connectionAckReceived);
}

void UDPHolePunchingConnectionInitiationFsm::noConnectionAckOnTime()
{
    NX_ASSERT(isInSelfAioThread());

    NX_VERBOSE(this, "Connection %1. No connection ack has been reported on time", m_connectionId);

    changeState(Event::connectionAckWaitTimedOut);
}

void UDPHolePunchingConnectionInitiationFsm::relayInstanceResolveCompleted(
    nx::cloud::relay::api::ResultCode resultCode,
    nx::utils::Url relayInstanceUrl)
{
    NX_ASSERT(isInSelfAioThread());

    if (resultCode == nx::cloud::relay::api::ResultCode::ok)
    {
        NX_VERBOSE(this, "Connection %1. Found relay instance %2",
            m_connectionId, relayInstanceUrl);
        m_trafficRelayUrl = relayInstanceUrl.toString().toUtf8();
    }
    else
    {
        NX_WARNING(this, "Connection %1. Could not find a relay instance. %2",
            m_connectionId, nx::cloud::relay::api::toString(resultCode));
    }

    changeState(Event::relayResolveCompleted);
}

void UDPHolePunchingConnectionInitiationFsm::connectionResultReceived(
    api::ConnectionResultRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    NX_ASSERT(isInSelfAioThread());

    m_sessionStatisticsInfo.resultCode = request.resultCode;
    m_connectResultReport = std::move(request);

    completionHandler(api::ResultCode::ok);

    changeState(Event::connectionResultReceived);
}

void UDPHolePunchingConnectionInitiationFsm::connectionResultWaitTimedOut()
{
    changeState(Event::connectionResultWaitTimedOut);
}

//---------------------------------------------------------------------------------------------
// Actions

void UDPHolePunchingConnectionInitiationFsm::sendConnectionRequestedNotification()
{
    NX_VERBOSE(this, "Connection %1. sendConnectionRequestedNotification", m_connectionId);

    auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
    if (!serverConnectionStrongRef)
    {
        NX_VERBOSE(this, "Connection %1. Cannot send connection_requested to %2. "
            "Connection is broken", m_connectionId, m_serverPeerHostName);
        connectionRequestedNotificationFailed();
        return;
    }

    // TODO: #ak The following should be guaranteed at the compile time.
    NX_ASSERT(m_clientEndpoint);
    if (!m_clientEndpoint)
    {
        NX_VERBOSE(this, "Connection %1. Cannot send connection_requested to %2. "
            "Client address is unknown", m_connectionId, m_serverPeerHostName);
        connectionRequestedNotificationFailed();
        return;
    }

    auto indication = prepareConnectionRequestedIndication(
        *m_clientEndpoint,
        m_connectRequest);
    serverConnectionStrongRef->sendMessage(std::move(indication));
    // TODO #ak: Wait for sendMessage result.

    connectionRequestedNotificationSent();
}

void UDPHolePunchingConnectionInitiationFsm::startNoConnectionAckTimer()
{
    NX_VERBOSE(this, "Connection %1. startNoConnectionAckTimer", m_connectionId);

    m_timer.start(
        m_settings.connectionParameters().connectionAckAwaitTimeout,
        [this]() { noConnectionAckOnTime(); });
}

void UDPHolePunchingConnectionInitiationFsm::stopNoConnectionAckTimer()
{
    NX_VERBOSE(this, "Connection %1. stopNoConnectionAckTimer", m_connectionId);

    m_timer.pleaseStopSync();
}

void UDPHolePunchingConnectionInitiationFsm::sendConnectResponse()
{
    NX_ASSERT(isInSelfAioThread());

    if (!m_cachedConnectResponse)
    {
        m_cachedConnectResponse = prepareConnectResponse();
        fixConnectResponseForBuggyClient(
            std::get<0>(*m_cachedConnectResponse),
            &std::get<1>(*m_cachedConnectResponse));
    }

    NX_VERBOSE(this, "Connection %1. Sending connect response (%2, %3)",
        m_connectionId, QnLexical::serialized(std::get<0>(*m_cachedConnectResponse)),
        QJson::serialized(std::get<1>(*m_cachedConnectResponse)));

    NX_ASSERT(!m_connectResponseSenders.empty(),
        "State %1. Response: (%2, %3)", m_state,
            QnLexical::serialized(std::get<0>(*m_cachedConnectResponse)),
            QJson::serialized(std::get<1>(*m_cachedConnectResponse)));

    for (auto& senderContext: m_connectResponseSenders)
        sendConnectResponse(senderContext, *m_cachedConnectResponse);

    m_connectResponseSenders.clear();
}

void UDPHolePunchingConnectionInitiationFsm::sendConnectionAckResponse()
{
    NX_VERBOSE(this, "Connection %1. sendConnectionAckResponse", m_connectionId);

    if (m_connectionAckCompletionHandler)
        nx::utils::swapAndCall(m_connectionAckCompletionHandler, api::ResultCode::ok);
}

void UDPHolePunchingConnectionInitiationFsm::startConnectionResultWaitTimer()
{
    NX_VERBOSE(this, "Connection %1. startConnectionResultWaitTimer", m_connectionId);

    m_timer.start(
        m_settings.connectionParameters().connectionResultWaitTimeout,
        [this]() { connectionResultWaitTimedOut(); });
}

void UDPHolePunchingConnectionInitiationFsm::stopConnectionResultWaitTimer()
{
    NX_VERBOSE(this, "Connection %1. stopConnectionResultWaitTimer", m_connectionId);

    m_timer.pleaseStopSync();
}

void UDPHolePunchingConnectionInitiationFsm::exitFsmIfReportHasAlreadyBeenReceived()
{
    NX_VERBOSE(this, "Connection %1. exitFsmIfReportHasAlreadyBeenReceived", m_connectionId);

    if (m_connectResultReport)
        changeState(Event::connectionResultReceived);
}

void UDPHolePunchingConnectionInitiationFsm::reportFsmCompletion()
{
    NX_VERBOSE(this, "Connection %1. reportFsmCompletion", m_connectionId);

    if (!m_onFsmFinishedEventHandler)
        return;

    post(
        [resultCode = m_sessionStatisticsInfo.resultCode,
            handler = std::exchange(m_onFsmFinishedEventHandler, nullptr)]()
        {
            handler(resultCode);
        });
}

//---------------------------------------------------------------------------------------------

void UDPHolePunchingConnectionInitiationFsm::updateSessionStatistics(
    const RequestSourceDescriptor& requestSourceDescriptor,
    const api::ConnectRequest& connectRequest)
{
    m_sessionStatisticsInfo.originatingHostEndpoint =
        requestSourceDescriptor.sourceAddress.toString().toUtf8();
    m_sessionStatisticsInfo.originatingHostName = connectRequest.originatingPeerId;
    m_sessionStatisticsInfo.destinationHostName = connectRequest.destinationHostName;
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
            connectionRequestedEvent.udpEndpointList.insert(
                connectionRequestedEvent.udpEndpointList.begin(),
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
    connectionRequestedEvent.cloudConnectVersion = connectRequest.cloudConnectVersion;
    connectionRequestedEvent.serialize(&indication);

    return indication;
}

void UDPHolePunchingConnectionInitiationFsm::initiateRelayInstanceSearch()
{
    findRelayInstance([this](auto&&... args) {
        relayInstanceResolveCompleted(std::forward<decltype(args)>(args)...); });
}

void UDPHolePunchingConnectionInitiationFsm::findRelayInstance(
    RelayInstanceSearchCompletionHandler handler)
{
    auto sharedHandler =
        std::make_shared<RelayInstanceSearchCompletionHandler>(std::move(handler));

    m_findRelayInstanceFunc.invokeWithTimeout(
        [sharedHandler](auto... args)
        {
            nx::utils::swapAndCall(*sharedHandler, std::move(args)...);
        },
        m_settings.connectionParameters().maxRelayInstanceSearchTime,
        [sharedHandler]()
        {
            nx::utils::swapAndCall(
                *sharedHandler,
                nx::cloud::relay::api::ResultCode::timedOut,
                nx::utils::Url());
        },
        m_relayClusterClient,
        m_serverPeerHostName.toStdString(),
        m_clientEndpoint->sourceAddress.address);
}

std::tuple<api::ResultCode, api::ConnectResponse>
    UDPHolePunchingConnectionInitiationFsm::prepareConnectResponse()
{
    std::vector<network::SocketAddress> tcpEndpoints =
        m_connectionAckRequest.forwardedTcpEndpointList;
    tcpEndpoints.insert(
        tcpEndpoints.begin(),
        m_serverPeerEndpoints.begin(), m_serverPeerEndpoints.end());

    api::ConnectResponse connectResponse;
    connectResponse.params = m_settings.connectionParameters();
    connectResponse.udpEndpointList = m_connectionAckRequest.udpEndpointList;
    connectResponse.forwardedTcpEndpointList = std::move(tcpEndpoints);
    connectResponse.cloudConnectVersion = m_serverPeerCloudConnectVersion;
    connectResponse.destinationHostFullName = m_serverPeerHostName;
    connectResponse.trafficRelayUrl = m_trafficRelayUrl;

    api::ResultCode resultCode = api::ResultCode::ok;

    if (connectResponse.udpEndpointList.empty() &&
        connectResponse.forwardedTcpEndpointList.empty() &&
        !connectResponse.trafficRelayUrl)
    {
        NX_DEBUG(this, "Connection %1. No connect method detected", m_connectionId);
        resultCode = api::ResultCode::noSuitableConnectionMethod;
    }

    return std::make_tuple(resultCode, std::move(connectResponse));
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

void UDPHolePunchingConnectionInitiationFsm::sendConnectResponse(
    ConnectResponseSenderContext& senderContext,
    const std::tuple<api::ResultCode, api::ConnectResponse>& response)
{
    if (senderContext.networkProtocol == network::TransportProtocol::udp)
    {
        // Sending the request as-is.
        nx::utils::swapAndCall(
            senderContext.sendResponseFunc,
            std::get<0>(response),
            std::get<1>(response));
    }
    else if (senderContext.networkProtocol == network::TransportProtocol::tcp)
    {
        // Removing server's UDP endpoints which could be present in the response
        // in case if the corresponding connect request was a retransmit.
        auto responseWithoutUdpEndpoints = std::get<1>(response);
        responseWithoutUdpEndpoints.udpEndpointList.clear();
        nx::utils::swapAndCall(
            senderContext.sendResponseFunc,
            std::get<0>(response),
            std::move(responseWithoutUdpEndpoints));
    }
    else
    {
        NX_ASSERT(false, lm("Protocol: %1").args((int) senderContext.networkProtocol));
    }
}

//-------------------------------------------------------------------------------------------------

constexpr const char* toString(State state)
{
    switch (state)
    {
        case State::init:
            return "init";
        case State::sendingConnectionRequestedIndication:
            return "sendingConnectionRequestedIndication";
        case State::waitingServerPeerUdpAddress:
            return "waitingServerPeerUdpAddress";
        case State::resolvingServersRelayInstance:
            return "resolvingServersRelayInstance";
        case State::waitingConnectionResult:
            return "waitingConnectionResult";
        case State::fini:
            return "fini";
    }

    return "unknown";
}

constexpr const char* toString(Event event)
{
    switch (event)
    {
        case Event::connectOverUdpRequestReceived:
            return "connectOverUdpRequestReceived";
        case Event::connectOverTcpRequestReceived:
            return "connectOverTcpRequestReceived";
        case Event::connectionRequestedSent:
            return "connectionRequestedSent";
        case Event::connectionRequestedSendFailed:
            return "connectionRequestedSendFailed";
        case Event::connectionAckReceived:
            return "connectionAckReceived";
        case Event::connectionAckWaitTimedOut:
            return "connectionAckWaitTimedOut";
        case Event::relayResolveCompleted:
            return "relayResolveCompleted";
        case Event::connectionResultReceived:
            return "connectionResultReceived";
        case Event::connectionResultWaitTimedOut:
            return "connectionResultWaitTimedOut";
    }

    return "unknown";
}

} // namespace hpm
} // namespace nx
