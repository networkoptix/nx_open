/**********************************************************
* Jan 15, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_connection_initiation_fsm.h"


namespace nx {
namespace hpm {

UDPHolePunchingConnectionInitiationFsm::UDPHolePunchingConnectionInitiationFsm(
    nx::String connectionID,
    std::function<void()> onFsmFinishedEventHandler)
:
    m_state(State::init),
    m_connectionID(std::move(connectionID)),
    m_onFsmFinishedEventHandler(std::move(onFsmFinishedEventHandler)),
    m_timer(SocketFactory::createStreamSocket())
{
    auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
    if (!serverConnectionStrongRef)
    {
        m_timer->post(std::move(m_onFsmFinishedEventHandler));
        return;
    }

    m_timer->bindToAioThread(serverConnectionStrongRef);
    serverConnectionStrongRef->registerConnectionClosedHandler(
        std::bind(&UDPHolePunchingConnectionInitiationFsm::onServerConnectionClosed, this));
}

UDPHolePunchingConnectionInitiationFsm::UDPHolePunchingConnectionInitiationFsm(
    UDPHolePunchingConnectionInitiationFsm&& rhs)
:
    m_state(rhs.m_state),
    m_connectionID(std::move(rhs.m_connectionID)),
    m_onFsmFinishedEventHandler(std::move(rhs.m_onFsmFinishedEventHandler)),
    m_timer(std::move(rhs.m_timer))
{
}

UDPHolePunchingConnectionInitiationFsm& UDPHolePunchingConnectionInitiationFsm::operator=(
    UDPHolePunchingConnectionInitiationFsm&& rhs)
{
    if (this == &rhs)
        return *this;

    m_state = rhs.m_state;
    m_connectionID = std::move(rhs.m_connectionID);
    m_onFsmFinishedEventHandler = std::move(rhs.m_onFsmFinishedEventHandler);
    m_timer = std::move(m_timer);
    return *this;
}

void UDPHolePunchingConnectionInitiationFsm::onConnectRequest(
    const ConnectionStrongRef& connection,
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> connectResponseSender)
{
    api::ConnectionRequestedIndication connectionRequestedData;
    connectionRequestedData.connectionID = request.connectionID;
    connectionRequestedData.remoteUDPAddress = connection->getSourceAddress();
    connectionRequestedData.connectionMethod = ConnectionMethod::udpHolePunching;
    nx::stun::Message indication;
    connectionRequestedData.serialize(&indication);
 
    m_timer->dispatch([this, indication, connectResponseSender]()
    {
        m_connectResponseSender = std::move(connectResponseSender);
        auto serverConnectionStrongRef = m_serverConnectionWeakRef.lock();
        if (!serverConnectionStrongRef)
        {
            m_timer->post(std::bind(&UDPHolePunchingConnectionInitiationFsm::done, this, false));
            return;
        }
        serverConnectionStrongRef->sendMessage(std::move(indication));
        m_timer->registerTimer(
            UDP_HOLE_PUNCHING_SESSION_TIMEOUT,
            std::bind(&UDPHolePunchingConnectionInitiationFsm::sessionTimedout, this));
    });
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionAckRequest(
    api::ConnectionAckRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_timer->dispatch([this, request, completionHandler]()
    {
        api::ConnectResponse connectResponse;
        connectResponse.serverPeerEndpoint = request.udpEndpoint;
        auto responseSender = std::move(m_connectResponseSender);
        responseSender(api::ResultCode::ok, std::move(connectResponse));
    });
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionResultRequest(
    api::ConnectionResultRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
}

void UDPHolePunchingConnectionInitiationFsm::sessionTimedout()
{
    
}

void UDPHolePunchingConnectionInitiationFsm::onServerConnectionClosed()
{
}

void UDPHolePunchingConnectionInitiationFsm::done(bool result)
{
    //TODO sending connect response
    auto onFinishedHandler = std::move(m_onFsmFinishedEventHandler);
    onFinishedHandler(result);
}

} // namespace hpm
} // namespace nx
