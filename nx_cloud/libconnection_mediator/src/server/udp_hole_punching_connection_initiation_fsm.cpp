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

const std::chrono::seconds kUdpHolePunchingSessionTimeout(7);
const std::chrono::seconds kConnectionResultWaitTimeout(15);

UDPHolePunchingConnectionInitiationFsm::UDPHolePunchingConnectionInitiationFsm(
    nx::String connectionID,
    const ListeningPeerPool::ConstDataLocker& serverPeerDataLocker,
    std::function<void(api::ResultCode)> onFsmFinishedEventHandler,
    const conf::Settings& settings)
:
    m_state(State::init),
    m_connectionID(std::move(connectionID)),
    m_onFsmFinishedEventHandler(std::move(onFsmFinishedEventHandler)),
    m_settings(settings),
    m_serverConnectionWeakRef(serverPeerDataLocker.value().peerConnection),
    m_directTcpAddresses(serverPeerDataLocker.value().endpoints)
{
    auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
    if (!serverConnectionStrongRef)
    {
        m_timer.post(std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::serverConnectionBroken));
        return;
    }

    m_sessionStatisticsInfo.startTime = nx::utils::utcTime();
    m_sessionStatisticsInfo.sessionId = connectionID;

    m_timer.bindToAioThread(serverConnectionStrongRef->socket()->getAioThread());
}

void UDPHolePunchingConnectionInitiationFsm::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_timer.pleaseStop(std::move(completionHandler));
}

void UDPHolePunchingConnectionInitiationFsm::onConnectRequest(
    const ConnectionStrongRef& originatingPeerConnection,
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> connectResponseSender)
{
    m_timer.dispatch(
        [this, originatingPeerConnection,
            request = std::move(request), connectResponseSender]()
        {
            m_sessionStatisticsInfo.originatingHostEndpoint = 
                originatingPeerConnection->getSourceAddress().toString().toUtf8();
            m_sessionStatisticsInfo.originatingHostName = request.originatingPeerId;
            m_sessionStatisticsInfo.destinationHostName = request.destinationHostName;

            api::ConnectionRequestedEvent connectionRequestedEvent;
            connectionRequestedEvent.connectSessionId = std::move(request.connectSessionId);
            connectionRequestedEvent.originatingPeerID = std::move(request.originatingPeerId);
            std::move(
                request.udpEndpointList.begin(),
                request.udpEndpointList.end(),
                std::back_inserter(connectionRequestedEvent.udpEndpointList));
            const auto originatingPeerSourceAddress = originatingPeerConnection->getSourceAddress();
            //setting ports if needed
            for (auto& endpoint: connectionRequestedEvent.udpEndpointList)
            {
                if (endpoint.port == 0)
                    endpoint.port = originatingPeerSourceAddress.port;
            }
            if (!request.ignoreSourceAddress)
                connectionRequestedEvent.udpEndpointList.emplace_front(
                    originatingPeerSourceAddress);
            connectionRequestedEvent.connectionMethods =
                api::ConnectionMethod::udpHolePunching;
            connectionRequestedEvent.params = m_settings.connectionParameters();
            nx::stun::Message indication(
                stun::Header(
                    stun::MessageClass::indication,
                    stun::cc::indications::connectionRequested));
            connectionRequestedEvent.serialize(&indication);
            connectionRequestedEvent.cloudConnectVersion = request.cloudConnectVersion;

            NX_ASSERT(connectResponseSender);
            m_connectResponseSender = std::move(connectResponseSender);
            auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
            if (!serverConnectionStrongRef)
            {
                m_timer.post(std::bind(
                    &UDPHolePunchingConnectionInitiationFsm::done,
                    this,
                    api::ResultCode::serverConnectionBroken));
                return;
            }
            serverConnectionStrongRef->sendMessage(std::move(indication));  //TODO #ak check sendMessage result
            m_timer.start(
                kUdpHolePunchingSessionTimeout,
                std::bind(
                    &UDPHolePunchingConnectionInitiationFsm::done,
                    this,
                    api::ResultCode::noReplyFromServer));
            m_state = State::waitingServerPeerUDPAddress;
        });
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionAckRequest(
    const ConnectionStrongRef& connection,
    api::ConnectionAckRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_timer.dispatch(
        [this, connection, request = std::move(request),
            completionHandler = std::move(completionHandler)]() mutable
        {
            m_sessionStatisticsInfo.destinationHostEndpoint = 
                connection->getSourceAddress().toString().toUtf8();
            
            if (m_state > State::waitingServerPeerUDPAddress)
            {
                NX_LOGX(
                    lm("Connection %1. Received connectionAck while in %2 state. Ignoring...")
                        .arg(m_connectionID).arg(static_cast<int>(m_state)),
                    cl_logDEBUG1);
                completionHandler(api::ResultCode::ok);
                return;
            }

            NX_ASSERT(m_connectResponseSender);

            if (connection->transportProtocol() == nx::network::TransportProtocol::udp)
                request.udpEndpointList.push_front(connection->getSourceAddress());

            if (request.udpEndpointList.empty() && request.forwardedTcpEndpointList.empty())
            {
                completionHandler(api::ResultCode::noSuitableConnectionMethod);
                m_timer.post(std::bind(
                    &UDPHolePunchingConnectionInitiationFsm::done,
                    this,
                    api::ResultCode::noSuitableConnectionMethod));
                return;
            }

            auto connectResponseSender = std::move(m_connectResponseSender);

            auto tcpEndpoints = std::move(request.forwardedTcpEndpointList);
            tcpEndpoints.insert(
                tcpEndpoints.begin(), m_directTcpAddresses.begin(), m_directTcpAddresses.end());

            api::ConnectResponse connectResponse;
            connectResponse.params = m_settings.connectionParameters();
            connectResponse.udpEndpointList = std::move(request.udpEndpointList);
            connectResponse.forwardedTcpEndpointList = std::move(tcpEndpoints);
            connectResponse.cloudConnectVersion = request.cloudConnectVersion;

            m_state = State::waitingConnectionResult;
            connectResponseSender(
                api::ResultCode::ok,
                std::move(connectResponse));
            completionHandler(api::ResultCode::ok);
            m_timer.start(
                kConnectionResultWaitTimeout,
                std::bind(
                    &UDPHolePunchingConnectionInitiationFsm::done,
                    this,
                    api::ResultCode::timedOut));
        });
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionResultRequest(
    api::ConnectionResultRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_timer.dispatch(
        [this, request, completionHandler = std::move(completionHandler)]()
        {
            completionHandler(api::ResultCode::ok);
            m_timer.post(std::bind(
                &UDPHolePunchingConnectionInitiationFsm::done,
                this,
                api::ResultCode::ok));
        });
}

stats::ConnectSession UDPHolePunchingConnectionInitiationFsm::statisticsInfo() const
{
    return m_sessionStatisticsInfo;
}

void UDPHolePunchingConnectionInitiationFsm::done(api::ResultCode result)
{
    if (m_state < State::waitingConnectionResult)
    {
        NX_ASSERT(result != api::ResultCode::ok);
        auto connectResponseSender = std::move(m_connectResponseSender);
        connectResponseSender(
            result,
            api::ConnectResponse());

        //TODO #ak saving connection result
    }
    m_state = State::fini;

    auto onFinishedHandler = std::move(m_onFsmFinishedEventHandler);
    onFinishedHandler(result);
}

} // namespace hpm
} // namespace nx
