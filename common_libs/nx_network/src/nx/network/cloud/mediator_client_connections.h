#pragma once

#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/stun/udp_client.h>

#include "base_mediator_client.h"
#include "data/client_bind_data.h"
#include "data/connect_data.h"
#include "data/connection_result_data.h"
#include "data/resolve_domain_data.h"
#include "data/resolve_peer_data.h"

namespace nx {
namespace hpm {
namespace api {

/**
 * Provides access to mediator functions to be used by clients.
 * @note These requests DO NOT require authentication.
 */
template<class NetworkClientType>
class MediatorClientConnection:
    public BaseMediatorClient<NetworkClientType>
{
public:
    template<typename ... Args>
    MediatorClientConnection(Args ... args):
        BaseMediatorClient<NetworkClientType>(std::forward<Args>(args) ...)
    {
    }

    template<typename Request>
    void send(Request request, utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> handler)
    {
        this->doRequest(std::move(request), std::move(handler));
    }

    void resolveDomain(
        nx::hpm::api::ResolveDomainRequest resolveData,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolveDomainResponse)> completionHandler)
    {
        this->doRequest(
            std::move(resolveData),
            std::move(completionHandler));
    }

    void resolvePeer(
        nx::hpm::api::ResolvePeerRequest resolveData,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolvePeerResponse)> completionHandler)
    {
        this->doRequest(
            std::move(resolveData),
            std::move(completionHandler));
    }

    void connect(
        nx::hpm::api::ConnectRequest connectData,
        utils::MoveOnlyFunc<void(
            stun::TransportHeader /*stunTransportHeader*/,
            nx::hpm::api::ResultCode,
            nx::hpm::api::ConnectResponse)> completionHandler)
    {
        this->doRequest(
            std::move(connectData),
            std::move(completionHandler));
    }

    void bind(
        nx::hpm::api::ClientBindRequest request,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ClientBindResponse)> completionHandler)
    {
        this->doRequest(
            std::move(request),
            std::move(completionHandler));
    }
};

typedef MediatorClientConnection<stun::AsyncClientUser> MediatorClientTcpConnection;
typedef MediatorClientConnection<stun::UdpClient> MediatorClientUdpConnection;

} // namespace api
} // namespace hpm
} // namespace nx
