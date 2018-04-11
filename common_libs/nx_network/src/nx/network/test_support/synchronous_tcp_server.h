#pragma once

#include <atomic>
#include <memory>

#include <nx/network/abstract_socket.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/byte_stream/pipeline.h>

namespace nx {
namespace network {
namespace test {

class NX_NETWORK_API SynchronousStreamSocketServer
{
public:
    /**
     * Initializes regular TCP v4 server socket.
     */
    SynchronousStreamSocketServer();
    SynchronousStreamSocketServer(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket);
    virtual ~SynchronousStreamSocketServer();

    bool bindAndListen(const SocketAddress& endpoint);
    SocketAddress endpoint() const;
    void start();
    void stop();

    void waitForAtLeastOneConnection();
    AbstractStreamSocket* anyConnection();

protected:
    virtual void processConnection(AbstractStreamSocket* connection) = 0;

    bool isStopped() const;

private:
    nx::utils::thread m_thread;
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    std::atomic<bool> m_stopped;
    nx::utils::AtomicUniquePtr<AbstractStreamSocket> m_connection;

    void threadMain();
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API SynchronousReceivingServer:
    public SynchronousStreamSocketServer
{
    using base_type = SynchronousStreamSocketServer;

public:
    SynchronousReceivingServer(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket,
        utils::bstream::AbstractOutput* synchronousServerReceivedData);

protected:
    virtual void processConnection(AbstractStreamSocket* connection) override;

private:
    utils::bstream::AbstractOutput* m_synchronousServerReceivedData = nullptr;
};

} // namespace test
} // namespace network
} // namespace nx
