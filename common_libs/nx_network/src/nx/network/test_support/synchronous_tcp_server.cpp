#include "synchronous_tcp_server.h"

#include <nx/network/system_socket.h>

namespace nx {
namespace network {
namespace test {

SynchronousStreamSocketServer::SynchronousStreamSocketServer():
    SynchronousStreamSocketServer(std::make_unique<nx::network::TCPServerSocket>(AF_INET))
{
}

SynchronousStreamSocketServer::SynchronousStreamSocketServer(
    std::unique_ptr<AbstractStreamServerSocket> serverSocket)
    :
    m_serverSocket(std::move(serverSocket)),
    m_stopped(false)
{
}

SynchronousStreamSocketServer::~SynchronousStreamSocketServer()
{
    stop();
}

bool SynchronousStreamSocketServer::bindAndListen(const SocketAddress& endpoint)
{
    return m_serverSocket->setRecvTimeout(100)
        && m_serverSocket->bind(endpoint)
        && m_serverSocket->listen();
}

SocketAddress SynchronousStreamSocketServer::endpoint() const
{
    return m_serverSocket->getLocalAddress();
}

void SynchronousStreamSocketServer::start()
{
    m_thread = nx::utils::thread(std::bind(&SynchronousStreamSocketServer::threadMain, this));
}

void SynchronousStreamSocketServer::stop()
{
    m_stopped = true;
    if (m_thread.joinable())
        m_thread.join();
}

void SynchronousStreamSocketServer::waitForAtLeastOneConnection()
{
    while (!m_connection)
        std::this_thread::yield();
}

AbstractStreamSocket* SynchronousStreamSocketServer::anyConnection()
{
    return m_connection.get();
}

bool SynchronousStreamSocketServer::isStopped() const
{
    return m_stopped;
}

void SynchronousStreamSocketServer::threadMain()
{
    while (!m_stopped)
    {
        m_connection.reset(m_serverSocket->accept());
        if (!m_connection)
            continue;
        processConnection(m_connection.get());
        m_connection.reset();
    }
}

//-------------------------------------------------------------------------------------------------

SynchronousReceivingServer::SynchronousReceivingServer(
    std::unique_ptr<AbstractStreamServerSocket> serverSocket,
    utils::bstream::AbstractOutput* synchronousServerReceivedData)
    :
    base_type(std::move(serverSocket)),
    m_synchronousServerReceivedData(synchronousServerReceivedData)
{
}

void SynchronousReceivingServer::processConnection(AbstractStreamSocket* connection)
{
    if (!connection->setRecvTimeout(std::chrono::milliseconds(1)))
        return;

    std::array<char, 256> readBuf;
    while (!isStopped())
    {
        int bytesRead = connection->recv(readBuf.data(), (unsigned int)readBuf.size(), 0);
        if (bytesRead > 0)
        {
            m_synchronousServerReceivedData->write(readBuf.data(), bytesRead);
            continue;
        }
        else if (bytesRead == 0)
        {
            break;
        }
        else
        {
            if (nx::network::socketCannotRecoverFromError(SystemError::getLastOSErrorCode()))
                break;
        }
    }
}

} // namespace test
} // namespace network
} // namespace nx
