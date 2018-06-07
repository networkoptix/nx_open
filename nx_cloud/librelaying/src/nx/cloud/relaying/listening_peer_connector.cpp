#include "listening_peer_connector.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cloud {
namespace relaying {

ListeningPeerConnector::ListeningPeerConnector(
    relaying::AbstractListeningPeerPool* listeningPeerPool)
    :
    m_listeningPeerPool(listeningPeerPool)
{
}

ListeningPeerConnector::~ListeningPeerConnector()
{
    m_guard.reset();
}

void ListeningPeerConnector::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
}

void ListeningPeerConnector::connectAsync(
    const network::SocketAddress& targetEndpoint,
    ConnectHandler handler)
{
    m_targetEndpoint = targetEndpoint;
    m_completionHandler = std::move(handler);

    post(std::bind(&ListeningPeerConnector::connectInAioThread, this));
}

void ListeningPeerConnector::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void ListeningPeerConnector::connectInAioThread()
{
    if (m_timeout)
    {
        m_timer.start(
            *m_timeout,
            [this]()
            {
                cancelTakeIdleConnection();
                reportFailure(SystemError::timedOut);
            });
    }

    m_listeningPeerPool->takeIdleConnection(
        relaying::ClientInfo(), //< TODO: #ak
        m_targetEndpoint.address.toString().toStdString(),
        [this, sharedGuard = m_guard.sharedGuard()](
            cloud::relay::api::ResultCode resultCode,
            std::unique_ptr<network::AbstractStreamSocket> connection)
        {
            auto callGuard = sharedGuard->lock();
            if (!callGuard)
                return;

            processTakeConnectionResult(resultCode, std::move(connection));
        });
}

void ListeningPeerConnector::cancelTakeIdleConnection()
{
    m_cancelled = true;
}

void ListeningPeerConnector::reportFailure(SystemError::ErrorCode systemErrorCode)
{
    nx::utils::swapAndCall(
        m_completionHandler,
        systemErrorCode,
        nullptr);
}

void ListeningPeerConnector::processTakeConnectionResult(
    cloud::relay::api::ResultCode resultCode,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    dispatch(
        [this, resultCode, connection = std::move(connection)]() mutable
        {
            if (m_cancelled)
                return;

            m_timer.pleaseStopSync();

            NX_VERBOSE(this, lm("Take connection to %1 finished with result %2")
                .args(m_targetEndpoint.address, QnLexical::serialized(resultCode)));

            nx::utils::swapAndCall(
                m_completionHandler,
                resultCode == cloud::relay::api::ResultCode::ok
                    ? SystemError::noError
                    : SystemError::hostUnreachable,
                std::move(connection));
        });
}

} // namespace relaying
} // namespace cloud
} // namespace nx
