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

void SynchronousTcpServer::threadMain()
{
    while (!m_stopped)
    {
        std::unique_ptr<AbstractStreamSocket> connection(m_serverSocket.accept());
        if (!connection)
            continue;
        processConnection(connection.get());
    }
}

} // namespace test
} // namespace network
} // namespace nx
