#include "target_peer_connector.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace cloud {
namespace gateway {

TargetPeerConnector::TargetPeerConnector(
    nx::cloud::relay::model::ListeningPeerPool* listeningPeerPool,
    const SocketAddress& targetEndpoint)
    :
    m_listeningPeerPool(listeningPeerPool),
    m_targetEndpoint(targetEndpoint)
{
}

void TargetPeerConnector::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_targetPeerSocket)
        m_targetPeerSocket->bindToAioThread(aioThread);
}

void TargetPeerConnector::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void TargetPeerConnector::connectAsync(ConnectHandler handler)
{
    using namespace std::placeholders;

    post(
        [this, handler = std::move(handler)]() mutable
        {
            m_targetPeerSocket = SocketFactory::createStreamSocket(false);
            m_targetPeerSocket->bindToAioThread(getAioThread());

            bool applySocketOptionResult = m_targetPeerSocket->setNonBlockingMode(true);
            if (applySocketOptionResult && m_timeout)
                applySocketOptionResult = m_targetPeerSocket->setSendTimeout(*m_timeout);
            if (!applySocketOptionResult)
            {
                const auto osErrorCode = SystemError::getLastOSErrorCode();
                NX_INFO(this, lm("Failed to set socket options. %1")
                    .arg(SystemError::toString(osErrorCode)));
                return handler(osErrorCode, nullptr);
            }
        
            m_completionHandler = std::move(handler);
            
            m_targetPeerSocket->connectAsync(
                m_targetEndpoint,
                std::bind(&TargetPeerConnector::processConnectionResult, this, _1));
        });
}

void TargetPeerConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    if (m_targetPeerSocket)
        m_targetPeerSocket.reset();
}

void TargetPeerConnector::processConnectionResult(
    SystemError::ErrorCode systemErrorCode)
{
    NX_VERBOSE(this, lm("Regular connect completed with result %1")
        .arg(SystemError::toString(systemErrorCode)));

    m_targetPeerSocket->cancelIOSync(nx::network::aio::etNone);
    nx::utils::swapAndCall(
        m_completionHandler,
        systemErrorCode,
        std::move(m_targetPeerSocket));
}

} // namespace gateway
} // namespace cloud
} // namespace nx
