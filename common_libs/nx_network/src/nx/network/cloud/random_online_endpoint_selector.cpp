#include "random_online_endpoint_selector.h"

#include <nx/utils/scope_guard.h>

namespace {

static const unsigned int CONNECT_TIMEOUT_MS = 14384;

} // namespace

namespace nx {
namespace network {
namespace cloud {

RandomOnlineEndpointSelector::RandomOnlineEndpointSelector():
    m_endpointResolved(false),
    m_socketsStillConnecting(0)
{
}

RandomOnlineEndpointSelector::~RandomOnlineEndpointSelector()
{
    decltype(m_sockets) sockets;
    {
        QnMutexLocker lk(&m_mutex);
        sockets = std::move(m_sockets);
    }
    for (auto& val: sockets)
        val.second->pleaseStopSync();
}

void RandomOnlineEndpointSelector::selectBestEndpont(
    const QString& /*moduleName*/,
    std::vector<SocketAddress> endpoints,
    std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler)
{
    QnMutexLocker lk(&m_mutex);

    m_handler = std::move(handler);
    m_endpointResolved = false;

    using namespace std::placeholders;
    //trying to establish connection to any endpoint and return first one that works
    for (auto& endpoint : endpoints)
    {
        auto sock = SocketFactory::createStreamSocket(
            false, NatTraversalSupport::disabled);
        if (!sock->setNonBlockingMode(true) ||
            !sock->setSendTimeout(CONNECT_TIMEOUT_MS))
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
    localHandler(nx_http::StatusCode::serviceUnavailable, SocketAddress());
}

void RandomOnlineEndpointSelector::done(
    AbstractStreamSocket* sock,
    SystemError::ErrorCode osErrorCode,
    SocketAddress endpoint)
{
    auto scopedGuard = makeScopeGuard([sock, this]() {
        QnMutexLocker lk(&m_mutex);
        m_sockets.erase(sock);
    });

    QnMutexLocker lk(&m_mutex);

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
        localHandler(nx_http::StatusCode::ok, std::move(endpoint));
    else
        localHandler(nx_http::StatusCode::serviceUnavailable, SocketAddress());
}

} // namespace cloud
} // namespace network
} // namespace nx
