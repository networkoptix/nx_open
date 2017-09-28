#pragma once

#include <atomic>

#include <nx/network/system_socket.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/std/thread.h>

namespace nx {
namespace network {
namespace test {

class NX_NETWORK_API SynchronousTcpServer
{
public:
    SynchronousTcpServer();
    virtual ~SynchronousTcpServer();

    bool bindAndListen(const SocketAddress& endpoint);
    SocketAddress endpoint() const;
    void start();
    void stop();

    void waitForAtLeastOneConnection();
    AbstractStreamSocket* anyConnection();

protected:
    virtual void processConnection(AbstractStreamSocket* connection) = 0;

private:
    nx::utils::thread m_thread;
    nx::network::TCPServerSocket m_serverSocket;
    std::atomic<bool> m_stopped;
    nx::utils::AtomicUniquePtr<AbstractStreamSocket> m_connection;

    void threadMain();
};

} // namespace test
} // namespace network
} // namespace nx
