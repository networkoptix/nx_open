#include "simple_message_server.h"

namespace nx {
namespace network {
namespace server {

SimpleMessageServerConnection::SimpleMessageServerConnection(
    StreamConnectionHolder<SimpleMessageServerConnection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> _socket,
    nx::Buffer request,
    nx::Buffer response)
    :
    m_socketServer(socketServer),
    m_socket(std::move(_socket)),
    m_request(std::move(request)),
    m_response(std::move(response)),
    m_creationTimestamp(std::chrono::steady_clock::now())
{
    bindToAioThread(m_socket->getAioThread());
}

void SimpleMessageServerConnection::startReadingConnection(
    std::optional<std::chrono::milliseconds> /*inactivityTimeout*/)
{
    using namespace std::placeholders;

    if (!m_socket->setNonBlockingMode(true))
        return m_socketServer->closeConnection(SystemError::getLastOSErrorCode(), this);

    if (m_request.isEmpty())
    {
        scheduleMessageSend();
    }
    else
    {
        m_readBuffer.reserve(m_request.size());
        // Reading request.
        m_socket->readAsyncAtLeast(
            &m_readBuffer, m_request.size(),
            std::bind(&SimpleMessageServerConnection::onDataRead, this, _1, _2));
    }
}

void SimpleMessageServerConnection::setKeepConnection(bool val)
{
    m_keepConnection = val;
}

std::chrono::milliseconds SimpleMessageServerConnection::lifeDuration() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now() - m_creationTimestamp);
}

int SimpleMessageServerConnection::messagesReceivedCount() const
{
    return 1;
}

void SimpleMessageServerConnection::stopWhileInAioThread()
{
    m_socket.reset();
}

void SimpleMessageServerConnection::onDataRead(
    SystemError::ErrorCode errorCode,
    size_t /*bytesRead*/)
{
    using namespace std::placeholders;

    if (errorCode != SystemError::noError)
    {
        NX_VERBOSE(this, lm("Failure while reading connection from %1. %2")
            .args(m_socket->getForeignAddress(), SystemError::toString(errorCode)));
        m_socketServer->closeConnection(errorCode, this);
        return;
    }

    if (!m_readBuffer.startsWith(m_request))
    {
        NX_VERBOSE(this, lm("Received unexpected message %1 from %2 while expecting %3")
            .args(m_readBuffer, m_socket->getForeignAddress(), m_request));
        m_socketServer->closeConnection(SystemError::invalidData, this);
        return;
    }

    scheduleMessageSend();

    if (m_keepConnection)
    {
        m_readBuffer.resize(0);
        m_readBuffer.reserve(m_request.size());
        // Reading request
        m_socket->readAsyncAtLeast(
            &m_readBuffer, m_request.size(),
            std::bind(&SimpleMessageServerConnection::onDataRead, this, _1, _2));
    }
}

void SimpleMessageServerConnection::scheduleMessageSend()
{
    m_sendQueue.push(m_response);
    if (m_sendQueue.size() == 1)
        sendNextMessage();
}

void SimpleMessageServerConnection::sendNextMessage()
{
    using namespace std::placeholders;

    m_socket->sendAsync(
        m_sendQueue.front(),
        std::bind(&SimpleMessageServerConnection::onDataSent, this, _1, _2));
}

void SimpleMessageServerConnection::onDataSent(
    SystemError::ErrorCode errorCode,
    size_t /*bytesSent*/)
{
    m_sendQueue.pop();

    if (errorCode != SystemError::noError)
    {
        NX_VERBOSE(this, lm("Failed to send to %1. %2")
            .arg(m_socket->getForeignAddress())
            .arg(SystemError::toString(errorCode)));
    }

    if (!m_keepConnection)
    {
        m_socketServer->closeConnection(errorCode, this);
    }
    else
    {
        if (!m_sendQueue.empty())
            sendNextMessage();
    }
}

} // namespace server
} // namespace network
} // namespace nx
