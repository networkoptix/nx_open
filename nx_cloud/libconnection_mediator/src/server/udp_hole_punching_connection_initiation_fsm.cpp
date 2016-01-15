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
    m_onFsmFinishedEventHandler(std::move(onFsmFinishedEventHandler))
{
}

UDPHolePunchingConnectionInitiationFsm::UDPHolePunchingConnectionInitiationFsm(
    UDPHolePunchingConnectionInitiationFsm&& rhs)
:
    m_state(rhs.m_state),
    m_connectionID(std::move(rhs.m_connectionID)),
    m_onFsmFinishedEventHandler(std::move(rhs.m_onFsmFinishedEventHandler))
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
    return *this;
}

void UDPHolePunchingConnectionInitiationFsm::onConnectRequest(
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler)
{
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionAckRequest(
    api::ConnectRequest request,
    std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler)
{
}

void UDPHolePunchingConnectionInitiationFsm::onConnectionResultRequest(
    api::ConnectionResultRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
}

} // namespace hpm
} // namespace nx
