// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>

#include <nx/network/abstract_socket.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/std/thread.h>

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

/**
 * Reads socket and pushes all received data to nx::utils::bstream::AbstractOutput.
 */
class NX_NETWORK_API SynchronousReceivingServer:
    public BasicSynchronousReceivingServer
{
    using base_type = BasicSynchronousReceivingServer;

public:
    SynchronousReceivingServer(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket,
        nx::utils::bstream::AbstractOutput* synchronousServerReceivedData);

protected:
    virtual void processDataReceived(
        AbstractStreamSocket* connection,
        const char* data,
        int dataSize) override;

private:
    nx::utils::bstream::AbstractOutput* m_synchronousServerReceivedData = nullptr;
};

//-------------------------------------------------------------------------------------------------

/**
 * Waits for a ping message on connection and responds with a pong message.
 */
class NX_NETWORK_API SynchronousPingPongServer:
    public BasicSynchronousReceivingServer
{
    using base_type = BasicSynchronousReceivingServer;

public:
    SynchronousPingPongServer(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket,
        const nx::Buffer& ping,
        const nx::Buffer& pong);

protected:
    virtual void processDataReceived(
        AbstractStreamSocket* connection,
        const char* data,
        int dataSize) override;

private:
    const nx::Buffer m_ping;
    const nx::Buffer m_pong;
};

//-------------------------------------------------------------------------------------------------

/**
 * Starts sending an infinite stream of random data after receiving any data on the connection.
 */
class NX_NETWORK_API SynchronousSpamServer:
    public BasicSynchronousReceivingServer
{
    using base_type = BasicSynchronousReceivingServer;

public:
    SynchronousSpamServer(
        std::unique_ptr<AbstractStreamServerSocket> serverSocket);

protected:
    virtual void processDataReceived(
        AbstractStreamSocket* connection,
        const char* data,
        int dataSize) override;
};

} // namespace test
} // namespace network
} // namespace nx
