#include "synchronous_tcp_server.h"

namespace nx {
namespace network {
namespace test {

SynchronousTcpServer::SynchronousTcpServer():
    m_stopped(false)
{
}

SynchronousTcpServer::~SynchronousTcpServer()
{
    stop();
}

bool SynchronousTcpServer::bindAndListen(const SocketAddress& endpoint)
{
    return m_serverSocket.setRecvTimeout(100)
        && m_serverSocket.bind(endpoint)
        && m_serverSocket.listen();
}

SocketAddress SynchronousTcpServer::endpoint() const
{
    return m_serverSocket.getLocalAddress();
}

void SynchronousTcpServer::start()
{
    m_thread = nx::utils::thread(std::bind(&SynchronousTcpServer::threadMain, this));
}

void SynchronousTcpServer::stop()
{
    m_stopped = true;
    if (m_thread.joinable())
        m_thread.join();
}

void SynchronousTcpServer::waitForAtLeastOneConnection()
{
    while (!m_connection)
        std::this_thread::yield();
}

AbstractStreamSocket* SynchronousTcpServer::anyConnection()
{
    return m_connection.get();
}

void SynchronousTcpServer::threadMain()
{
    while (!m_stopped)
    {
        m_connection.reset(m_serverSocket.accept());
        if (!m_connection)
            continue;
        processConnection(m_connection.get());
        m_connection.reset();
    }
}

} // namespace test
} // namespace network
} // namespace nx
