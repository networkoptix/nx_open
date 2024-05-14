// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/stun/udp_client.h>

#include "base_mediator_client.h"
#include "data/connect_data.h"
#include "data/connection_result_data.h"
#include "data/resolve_domain_data.h"
#include "data/resolve_peer_data.h"

namespace nx::hpm::api {

template<class NetworkClientType>
class AbstractMediatorClientConnection:
    public BaseMediatorClient<NetworkClientType>
{
    using base_type = BaseMediatorClient<NetworkClientType>;

public:
    template<typename ... Args>
    AbstractMediatorClientConnection(Args ... args):
        base_type(std::forward<Args>(args) ...)
    {
    }

    virtual ~AbstractMediatorClientConnection() = default;

    virtual void resolveDomain(
        nx::hpm::api::ResolveDomainRequest resolveData,
        nx::utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolveDomainResponse)> completionHandler) = 0;

    virtual void resolvePeer(
        nx::hpm::api::ResolvePeerRequest resolveData,
        nx::utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolvePeerResponse)> completionHandler) = 0;

    virtual void connect(
        nx::hpm::api::ConnectRequest connectData,
        nx::utils::MoveOnlyFunc<void(
            network::stun::TransportHeader /*stunTransportHeader*/,
            nx::hpm::api::ResultCode,
            nx::hpm::api::ConnectResponse)> completionHandler) = 0;
};

class NX_NETWORK_API AbstractMediatorClientTcpConnection:
    public AbstractMediatorClientConnection<network::stun::AsyncClientUser>
{
    using base_type = AbstractMediatorClientConnection<network::stun::AsyncClientUser>;

public:
    template<typename ... Args>
    AbstractMediatorClientTcpConnection(Args ... args):
        base_type(std::forward<Args>(args) ...)
    {
    }
};

class NX_NETWORK_API AbstractMediatorClientUdpConnection:
    public AbstractMediatorClientConnection<network::stun::UdpClient>
{
    using base_type = AbstractMediatorClientConnection<network::stun::UdpClient>;

public:
    template<typename ... Args>
    AbstractMediatorClientUdpConnection(Args ... args):
        base_type(std::forward<Args>(args) ...)
    {
    }
};

/**
 * Provides access to mediator functions to be used by clients.
 * NOTE: These requests DO NOT require authentication.
 */
template<class AbstractClientType>
class MediatorClientConnection:
    public AbstractClientType
{
    using base_type = AbstractClientType;

public:
    template<typename ... Args>
    MediatorClientConnection(Args ... args):
        base_type(std::forward<Args>(args) ...)
    {
    }

    template<typename Request>
    void send(Request request, nx::utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> handler)
    {
        this->doRequest(std::move(request), std::move(handler));
    }

    virtual void resolveDomain(
        nx::hpm::api::ResolveDomainRequest resolveData,
        nx::utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolveDomainResponse)> completionHandler) override
    {
        this->doRequest(
            std::move(resolveData),
            std::move(completionHandler));
    }

    virtual void resolvePeer(
        nx::hpm::api::ResolvePeerRequest resolveData,
        nx::utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolvePeerResponse)> completionHandler) override
    {
        this->doRequest(
            std::move(resolveData),
            std::move(completionHandler));
    }

    virtual void connect(
        nx::hpm::api::ConnectRequest connectData,
        nx::utils::MoveOnlyFunc<void(
            network::stun::TransportHeader /*stunTransportHeader*/,
            nx::hpm::api::ResultCode,
            nx::hpm::api::ConnectResponse)> completionHandler) override
    {
        this->doRequest(
            std::move(connectData),
            std::move(completionHandler));
    }
};

using MediatorClientTcpConnection = MediatorClientConnection<AbstractMediatorClientTcpConnection>;
using MediatorClientUdpConnection = MediatorClientConnection<AbstractMediatorClientUdpConnection>;

} // namespace nx::hpm::api
