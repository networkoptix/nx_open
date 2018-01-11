#include "simple_message_server.h"

namespace nx {
namespace network {
namespace server {

SimpleMessageServerConnection::SimpleMessageServerConnection(
    StreamConnectionHolder<SimpleMessageServerConnection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> _socket,
    nx::Buffer staticMessage)
    :
    m_socketServer(socketServer),
    m_socket(std::move(_socket)),
    m_staticMessage(std::move(staticMessage)),
    m_creationTimestamp(std::chrono::steady_clock::now())
{
    bindToAioThread(m_socket->getAioThread());
}

void SimpleMessageServerConnection::startReadingConnection(
    boost::optional<std::chrono::milliseconds> /*inactivityTimeout*/)
{
    using namespace std::placeholders;

    if (!m_socket->setNonBlockingMode(true))
        return m_socketServer->closeConnection(SystemError::getLastOSErrorCode(), this);

    m_socket->sendAsync(
        m_staticMessage,
        std::bind(&SimpleMessageServerConnection::onDataSent, this, _1, _2));
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

void SimpleMessageServerConnection::onDataSent(
    SystemError::ErrorCode errorCode,
    size_t bytesSent)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("Failed to send to %1. %2")
            .arg(m_socket->getForeignAddress())
            .arg(SystemError::toString(errorCode)),
            cl_logDEBUG2);
    }

    if (!m_keepConnection)
        m_socketServer->closeConnection(errorCode, this);
}

} // namespace server
} // namespace network
} // namespace nx
