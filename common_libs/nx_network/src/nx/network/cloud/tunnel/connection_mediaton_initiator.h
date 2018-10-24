#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "../mediator_connector.h"

namespace nx::network::cloud {

/**
 * Initiates cloud connect session by sending "connect" request to the mediator.
 * NOTE: Request is sent via both UDP and TCP.
 */
class NX_NETWORK_API ConnectionMediationInitiator:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using Handler =
        nx::utils::MoveOnlyFunc<void(hpm::api::ResultCode, hpm::api::ConnectResponse)>;

    ConnectionMediationInitiator(
        const hpm::api::MediatorAddress& mediatorAddress,
        std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> mediatorUdpClient);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void go(
        const hpm::api::ConnectRequest& request,
        Handler handler);

    /**
     * NOTE: It is safe to invoke this method only within ConnectionMediationInitiator::go 
     * completion handler.
     */
    std::unique_ptr<UDPSocket> takeUdpSocket();

    /**
     * Mediator address can be changed during performing request: the request can be redirected to
     * another mediator instance.
     */
    hpm::api::MediatorAddress mediatorLocation() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    hpm::api::MediatorAddress m_mediatorAddress;
    Handler m_handler;
    std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> m_mediatorUdpClient;

    void initiateConnectOverUdp(hpm::api::ConnectRequest request);

    void handleResponseOverUdp(
        stun::TransportHeader stunTransportHeader,
        nx::hpm::api::ResultCode resultCode,
        nx::hpm::api::ConnectResponse response);
};

} // namespace nx::network::cloud
