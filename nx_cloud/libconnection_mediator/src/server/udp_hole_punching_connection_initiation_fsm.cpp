/**********************************************************
* Jan 15, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connection_initiation_fsm.h"

#include <chrono>

#include <nx/network/cloud/data/connection_ack_data.h>
#include <nx/network/cloud/data/connection_requested_event_data.h>


namespace nx {
namespace hpm {

const std::chrono::seconds kUdpHolePunchingSessionTimeout(7);
const std::chrono::seconds kConnectionResultWaitTimeout(15);

UDPHolePunchingConnectionInitiationFsm::UDPHolePunchingConnectionInitiationFsm(
    nx::String connectionID,
    const ListeningPeerPool::ConstDataLocker& serverPeerDataLocker,
    std::function<void(api::ResultCode)> onFsmFinishedEventHandler)
:
    m_state(State::init),
    m_connectionID(std::move(connectionID)),
    m_onFsmFinishedEventHandler(std::move(onFsmFinishedEventHandler)),
    m_timer(SocketFactory::createStreamSocket()),
    m_serverConnectionWeakRef(serverPeerDataLocker.value().peerConnection)
{
    auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
    if (!serverConnectionStrongRef)
    {
        m_timer->post(std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::serverConnectionBroken));
        return;
    }

    m_timer->bindToAioThread(serverConnectionStrongRef->socket()->getAioThread());
}

void UDPHolePunchingConnectionInitiationFsm::onConnectRequest(
    const ConnectionStrongRef& originatingPeerConnection,
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> connectResponseSender)
{
    m_timer->dispatch([this, originatingPeerConnection, request, connectResponseSender]()  //TODO #ak msvc2015 move to lambda
    {
        api::ConnectionRequestedEvent connectionRequestedEvent;
        connectionRequestedEvent.connectSessionID = std::move(request.connectSessionID);
        connectionRequestedEvent.originatingPeerID = std::move(request.originatingPeerID);
        connectionRequestedEvent.udpEndpointList.emplace_back(
            originatingPeerConnection->getSourceAddress());
        connectionRequestedEvent.connectionMethods = api::ConnectionMethod::udpHolePunching;
        nx::stun::Message indication(
            stun::Header(
                stun::MessageClass::indication,
                stun::cc::indications::connectionRequested));
        connectionRequestedEvent.serialize(&indication);

        m_connectResponseSender = std::move(connectResponseSender);
        auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
        if (!serverConnectionStrongRef)
        {
            m_timer->post(std::bind(
                &UDPHolePunchingConnectionInitiationFsm::done,
                this,
                api::ResultCode::serverConnectionBroken));
            return;
        }
        serverConnectionStrongRef->sendMessage(std::move(indication));  //TODO #ak check sendMessage result
        m_timer->registerTimer(
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
    m_timer->dispatch([this, connection, request, completionHandler]() mutable  //TODO #msvc2015 move to lambda
    {
        assert(m_connectResponseSender);

        if (connection->transportProtocol() == nx::network::TransportProtocol::udp)
            request.udpEndpointList.push_front(connection->getSourceAddress());

        if (request.udpEndpointList.empty())
        {
            completionHandler(api::ResultCode::noSuitableConnectionMethod);
            m_timer->post(std::bind(
                &UDPHolePunchingConnectionInitiationFsm::done,
                this,
                api::ResultCode::noSuitableConnectionMethod));
            return;
        }

        auto connectResponseSender = std::move(m_connectResponseSender);

        api::ConnectResponse connectResponse;
        connectResponse.udpEndpointList = std::move(request.udpEndpointList);

        connectResponseSender(
            api::ResultCode::ok,
            std::move(connectResponse));
        completionHandler(api::ResultCode::ok);
        m_state = State::waitingConnectionResult;
        m_timer->registerTimer(
            kConnectionResultWaitTimeout,
            std::bind(
                &UDPHolePunchingConnectionInitiationFsm::done,
                this,
                api::ResultCode::timedout));
    });
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionResultRequest(
    api::ConnectionResultRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_timer->dispatch([this, request, completionHandler]()
    {
        //TODO #ak saving connection result
        completionHandler(api::ResultCode::ok);
        m_timer->post(std::bind(
            &UDPHolePunchingConnectionInitiationFsm::done,
            this,
            api::ResultCode::ok));
    });
}

void UDPHolePunchingConnectionInitiationFsm::done(api::ResultCode result)
{
    if (m_state < State::waitingConnectionResult)
    {
        Q_ASSERT(result != api::ResultCode::ok);
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
