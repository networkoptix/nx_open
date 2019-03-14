#include "target_peer_connector.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cloud {
namespace gateway {

TargetPeerConnector::TargetPeerConnector()
{
    bindToAioThread(getAioThread());
}

void TargetPeerConnector::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_targetPeerSocket)
        m_targetPeerSocket->bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

void TargetPeerConnector::connectAsync(
    const network::SocketAddress& targetEndpoint,
    ConnectHandler handler)
{
    m_targetEndpoint = targetEndpoint;

    post(
        [this, handler = std::move(handler)]() mutable
        {
            if (m_timeout)
            {
                m_timer.start(
                    *m_timeout,
                    std::bind(&TargetPeerConnector::interruptByTimeout, this));
            }

            m_completionHandler = std::move(handler);

            initiateDirectConnection();
        });
}

void TargetPeerConnector::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void TargetPeerConnector::setTargetConnectionRecvTimeout(
    std::optional<std::chrono::milliseconds> value)
{
    m_targetConnectionRecvTimeout = value;
}

void TargetPeerConnector::setTargetConnectionSendTimeout(
    std::optional<std::chrono::milliseconds> value)
{
    m_targetConnectionSendTimeout = value;
}

void TargetPeerConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    if (m_targetPeerSocket)
        m_targetPeerSocket.reset();
    m_timer.pleaseStopSync();
}

void TargetPeerConnector::initiateDirectConnection()
{
    using namespace std::placeholders;

    m_targetPeerSocket = nx::network::SocketFactory::createStreamSocket(false);
    m_targetPeerSocket->bindToAioThread(getAioThread());

    bool setSocketOptionsResult = m_targetPeerSocket->setNonBlockingMode(true);
    if (setSocketOptionsResult && m_timeout)
        setSocketOptionsResult = m_targetPeerSocket->setSendTimeout(*m_timeout);
    if (!setSocketOptionsResult)
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_INFO(this, lm("Failed to set socket options. %1")
            .args(SystemError::toString(osErrorCode)));
        return processDirectConnectionResult(osErrorCode);
    }

    m_targetPeerSocket->connectAsync(
        m_targetEndpoint,
        std::bind(&TargetPeerConnector::processDirectConnectionResult, this, _1));
}

void TargetPeerConnector::processDirectConnectionResult(
    SystemError::ErrorCode systemErrorCode)
{
    NX_VERBOSE(this, lm("Direct connect completed with result %1")
        .args(SystemError::toString(systemErrorCode)));

    m_targetPeerSocket->cancelIOSync(nx::network::aio::etNone);
    processConnectionResult(
        systemErrorCode,
        systemErrorCode == SystemError::noError ? std::move(m_targetPeerSocket) : nullptr);
}

void TargetPeerConnector::processConnectionResult(
    SystemError::ErrorCode systemErrorCode,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    m_timer.pleaseStopSync();

    if (connection)
    {
        if ((m_targetConnectionRecvTimeout &&
                !connection->setRecvTimeout(*m_targetConnectionRecvTimeout)) ||
            (m_targetConnectionSendTimeout &&
                !connection->setSendTimeout(*m_targetConnectionSendTimeout)))
        {
            systemErrorCode = SystemError::getLastOSErrorCode();
            NX_INFO(this, lm("Failed to set socket options. %1")
                .arg(SystemError::toString(systemErrorCode)));

            connection.reset();
        }
    }

    nx::utils::swapAndCall(
        m_completionHandler,
        systemErrorCode,
        std::move(connection));
}

void TargetPeerConnector::interruptByTimeout()
{
    NX_VERBOSE(this, lm("Connect to %1 failed by timeout.").args(m_targetEndpoint));
    m_targetPeerSocket.reset();
    processConnectionResult(SystemError::timedOut, nullptr);
}

} // namespace gateway
} // namespace cloud
} // namespace nx
