// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "random_online_endpoint_selector.h"

#include <nx/utils/scope_guard.h>

namespace nx::network::cloud {

static constexpr auto kDefaultConnectTimeout = std::chrono::milliseconds(14384);

} // namespace

namespace nx::network::cloud {

RandomOnlineEndpointSelector::~RandomOnlineEndpointSelector()
{
    decltype(m_sockets) sockets;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        sockets = std::move(m_sockets);
    }
    for (auto& val: sockets)
        val.second->pleaseStopSync();
}

void RandomOnlineEndpointSelector::selectBestEndpont(
    const std::string& /*moduleName*/,
    std::vector<SocketAddress> endpoints,
    std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> handler)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    m_handler = std::move(handler);
    m_endpointResolved = false;

    using namespace std::placeholders;
    //trying to establish connection to any endpoint and return first one that works
    for (auto& endpoint : endpoints)
    {
        auto sock = SocketFactory::createStreamSocket(
            ssl::kAcceptAnyCertificate, /*sslRequired*/ false, NatTraversalSupport::disabled);
        if (!sock->setNonBlockingMode(true) ||
            !sock->setSendTimeout(m_timeout ? *m_timeout : kDefaultConnectTimeout))
        {
            continue;
        }
        sock->connectAsync(
            endpoint,
            std::bind(&RandomOnlineEndpointSelector::done, this,
                sock.get(), _1, endpoint));
        m_sockets.emplace(sock.get(), std::move(sock));
    }

    m_socketsStillConnecting = m_sockets.size();
    if (!m_sockets.empty())
        return;
    //failed to start at least one connection. Reporting error...
    auto localHandler = std::move(m_handler);
    lk.unlock();
    localHandler(nx::network::http::StatusCode::serviceUnavailable, SocketAddress());
}

void RandomOnlineEndpointSelector::setTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_timeout = timeout;
}

void RandomOnlineEndpointSelector::done(
    AbstractStreamSocket* sock,
    SystemError::ErrorCode osErrorCode,
    SocketAddress endpoint)
{
    auto scopedGuard = nx::utils::makeScopeGuard([sock, this]() {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_sockets.erase(sock);
    });

    NX_MUTEX_LOCKER lk(&m_mutex);

    --m_socketsStillConnecting;
    if ((osErrorCode != SystemError::noError && m_socketsStillConnecting > 0) ||    //if all connections fail, we must invoke handler
        m_endpointResolved)
    {
        return;
    }

    m_endpointResolved = true;

    //selecting this endpoint
    auto localHandler = std::move(m_handler);
    lk.unlock();

    if (osErrorCode == SystemError::noError)
        localHandler(nx::network::http::StatusCode::ok, std::move(endpoint));
    else
        localHandler(nx::network::http::StatusCode::serviceUnavailable, SocketAddress());
}

} // namespace nx::network::cloud
