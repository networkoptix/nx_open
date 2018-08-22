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

class NX_NETWORK_API BasicSynchronousReceivingServer:
    public SynchronousStreamSocketServer
{
    using base_type = SynchronousStreamSocketServer;

public:
    BasicSynchronousReceivingServer(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket);

protected:
    virtual void processConnection(AbstractStreamSocket* connection) override;

    virtual void processDataReceived(
        AbstractStreamSocket* connection,
        const char* data,
        int dataSize) = 0;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API SynchronousReceivingServer:
    public BasicSynchronousReceivingServer
{
    using base_type = BasicSynchronousReceivingServer;

public:
    SynchronousReceivingServer(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket,
        utils::bstream::AbstractOutput* synchronousServerReceivedData);

protected:
    virtual void processDataReceived(
        AbstractStreamSocket* connection,
        const char* data,
        int dataSize) override;

private:
    utils::bstream::AbstractOutput* m_synchronousServerReceivedData = nullptr;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API SynchronousPingPongServer:
    public BasicSynchronousReceivingServer
{
    using base_type = BasicSynchronousReceivingServer;

public:
    SynchronousPingPongServer(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket,
        const std::string& ping,
        const std::string& pong);

protected:
    virtual void processDataReceived(
        AbstractStreamSocket* connection,
        const char* data,
        int dataSize) override;

private:
    const std::string m_ping;
    const std::string m_pong;
};

} // namespace test
} // namespace network
} // namespace nx
