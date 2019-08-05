#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>

#include "../mediator_connector.h"

namespace nx::network::cloud {

class CloudConnectSettings;

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
        const CloudConnectSettings& settings,
        const hpm::api::MediatorAddress& mediatorAddress,
        std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> mediatorUdpClient);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void start(
        const hpm::api::ConnectRequest& request,
        Handler handler);

    /**
     * NOTE: It is safe to invoke this method only within ConnectionMediationInitiator::start
     * completion handler.
     */
    std::unique_ptr<AbstractDatagramSocket> takeUdpSocket();

    /**
     * Mediator address can be changed during performing request: the request can be redirected to
     * another mediator instance.
     */
    hpm::api::MediatorAddress mediatorLocation() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const CloudConnectSettings& m_settings;
    hpm::api::MediatorAddress m_mediatorAddress;
    hpm::api::ConnectRequest m_request;
    Handler m_handler;
    std::unique_ptr<nx::hpm::api::MediatorClientUdpConnection> m_mediatorUdpClient;
    std::unique_ptr<AbstractDatagramSocket> m_udpHolePunchingSocket;
    aio::Timer m_tcpConnectDelayTimer;
    std::unique_ptr<nx::hpm::api::Client> m_mediatorApiClient;
    bool m_tcpRequestCompleted = false;

    void initiateConnectOverUdp();
    void initiateConnectOverTcp();

    void handleResponseOverUdp(
        stun::TransportHeader stunTransportHeader,
        nx::hpm::api::ResultCode resultCode,
        nx::hpm::api::ConnectResponse response);

    void handleResponse(
        nx::hpm::api::ResultCode resultCode,
        nx::hpm::api::ConnectResponse response);
};

} // namespace nx::network::cloud
