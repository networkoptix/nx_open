#include <gtest/gtest.h>

#include <nx/network/aio/async_channel_reflector.h>
#include <nx/network/ssl/ssl_stream_server_socket.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>

#include "sync_ssl_socket.h"

namespace nx {
namespace network {
namespace ssl {
namespace test {

namespace {

class AbstractReflectorPool
{
public:
    virtual ~AbstractReflectorPool() = default;

    virtual void add(std::unique_ptr<AbstractStreamSocket> streamSocket) = 0;
    virtual std::size_t count() const = 0;
    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() = 0;
};

class SynchronousReflector
{
public:
    SynchronousReflector(std::unique_ptr<AbstractStreamSocket> streamSocket):
        m_streamSocket(std::move(streamSocket)),
        m_terminated(false)
    {
    }

    ~SynchronousReflector()
    {
        stop();
    }

    void stop()
    {
        if (!m_terminated)
        {
            m_terminated = true;
            m_ioThread.join();
        }
    }

    void start()
    {
        m_ioThread = nx::utils::thread(
            std::bind(&SynchronousReflector::ioThreadMain, this));
    }

    std::unique_ptr<AbstractStreamSocket> takeSocket()
    {
        stop();

        decltype(m_streamSocket) result;
        result.swap(m_streamSocket);
        return result;
    }

private:
    nx::utils::thread m_ioThread;
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    std::atomic<bool> m_terminated;

    void ioThreadMain()
    {
        if (!m_streamSocket->setRecvTimeout(std::chrono::milliseconds(1)) ||
            !m_streamSocket->setSendTimeout(std::chrono::milliseconds(1)))
        {
            return;
        }

        while (!m_terminated)
        {
            std::array<char, 4 * 1024> readBuf;

            int bytesRead = m_streamSocket->recv(readBuf.data(), (int)readBuf.size());
            if (bytesRead <= 0)
            {
                if (SystemError::getLastOSErrorCode() == SystemError::timedOut)
                    continue;
                break;
            }

            int bytesWritten = m_streamSocket->send(readBuf.data(), bytesRead);
            if (bytesWritten != bytesRead)
            {
                if (SystemError::getLastOSErrorCode() == SystemError::timedOut)
                    continue;
                break;
            }
        }
    }
};

class SynchronousReflectorPool:
    public AbstractReflectorPool
{
public:
    virtual ~SynchronousReflectorPool()
    {
        for (auto& syncReflector: m_reflectors)
            syncReflector.reset();
    }

    virtual void add(std::unique_ptr<AbstractStreamSocket> streamSocket) override
    {
        ASSERT_TRUE(streamSocket->setNonBlockingMode(false));
        auto syncReflector = std::make_unique<SynchronousReflector>(std::move(streamSocket));
        syncReflector->start();
        m_reflectors.push_back(std::move(syncReflector));
    }

    virtual std::size_t count() const override
    {
        return m_reflectors.size();
    }

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override
    {
        auto result = m_reflectors.front()->takeSocket();
        m_reflectors.pop_front();
        return result;
    }

private:
    std::deque<std::unique_ptr<SynchronousReflector>> m_reflectors;
};

using AbstractStreamSocketAsyncReflector = aio::AsyncChannelReflector<AbstractStreamSocket>;

class AsynchronousReflectorPool:
    public AbstractReflectorPool
{
public:
    AsynchronousReflectorPool():
        m_terminated(false)
    {
    }

    virtual ~AsynchronousReflectorPool()
    {
        decltype (m_reflectors) acceptedConnections;
        {
            QnMutexLocker lock(&m_mutex);
            m_terminated = true;
            acceptedConnections.swap(m_reflectors);
        }

        for (auto& connection: acceptedConnections)
            connection->pleaseStopSync();
    }

    virtual void add(std::unique_ptr<AbstractStreamSocket> streamSocket) override
    {
        using namespace std::placeholders;

        ASSERT_TRUE(streamSocket->setNonBlockingMode(true));

        QnMutexLocker lock(&m_mutex);
        m_reflectors.push_back(nullptr);
        auto asyncReflector =
            std::make_unique<AbstractStreamSocketAsyncReflector>(
                std::move(streamSocket));
        asyncReflector->start(
            std::bind(&AsynchronousReflectorPool::onReflectorDone, this,
                --m_reflectors.end(), _1));
        m_reflectors.back() = std::move(asyncReflector);
    }

    virtual std::size_t count() const override
    {
        return m_reflectors.size();
    }

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override
    {
        auto result = m_reflectors.front()->takeChannel();
        m_reflectors.pop_front();
        return result;
    }

private:
    std::list<std::unique_ptr<AbstractStreamSocketAsyncReflector>> m_reflectors;
    bool m_terminated;
    QnMutex m_mutex;

    void onReflectorDone(
        std::list<std::unique_ptr<AbstractStreamSocketAsyncReflector>>::iterator reflectorIter,
        SystemError::ErrorCode /*sysErrorCode*/)
    {
        QnMutexLocker lock(&m_mutex);
        if (!m_terminated)
            m_reflectors.erase(reflectorIter);
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------
// Test fixture

/**
 * These tests use SyncSslSocket on one side to guarantee that ssl is actually implied in test.
 */
class SslSocketVerifySslIsActuallyUsed:
    public ::testing::Test
{
public:
    SslSocketVerifySslIsActuallyUsed():
        m_clientSocket(std::make_unique<TCPSocket>(AF_INET)),
        m_reflectorPoolInUse(nullptr)
    {
        switchToSynchronousMode();

        startAcceptor();
    }

    ~SslSocketVerifySslIsActuallyUsed()
    {
        stopAcceptor();
    }

    void switchToSynchronousMode()
    {
        m_reflectorPoolInUse = &m_synchronousReflectorPool;
    }

    void switchToAsynchronousMode()
    {
        m_reflectorPoolInUse = &m_asynchronousReflectorPool;
    }

protected:
    void givenEstablishedConnection()
    {
        ASSERT_TRUE(m_clientSocket.connect(m_acceptor->getLocalAddress(), nx::network::kNoTimeout));
    }

    void whenSentRandomData()
    {
        prepareTestData();

        ASSERT_EQ(
            m_testData.size(),
            m_clientSocket.send(m_testData.constData(), m_testData.size()));
    }

    void thenSameDataHasBeenReceivedInResponse()
    {
        nx::Buffer response;
        response.resize(m_testData.size());
        ASSERT_EQ(
            m_testData.size(),
            m_clientSocket.recv(response.data(), response.size(), MSG_WAITALL));

        ASSERT_EQ(m_testData, response);
    }

    void stopAcceptor()
    {
        if (m_acceptor)
            m_acceptor->pleaseStopSync();
    }

    AbstractReflectorPool* reflectorPoolInUse()
    {
        return m_reflectorPoolInUse;
    }

    AbstractReflectorPool* unusedReflectorPool()
    {
        return m_reflectorPoolInUse == &m_asynchronousReflectorPool
            ? static_cast<AbstractReflectorPool*>(&m_synchronousReflectorPool)
            : static_cast<AbstractReflectorPool*>(&m_asynchronousReflectorPool);
    }

private:
    std::unique_ptr<ssl::StreamServerSocket> m_acceptor;
    ClientSyncSslSocket m_clientSocket;
    nx::Buffer m_testData;
    AsynchronousReflectorPool m_asynchronousReflectorPool;
    SynchronousReflectorPool m_synchronousReflectorPool;
    AbstractReflectorPool* m_reflectorPoolInUse;

    void startAcceptor()
    {
        using namespace std::placeholders;

        m_acceptor = std::make_unique<ssl::StreamServerSocket>(
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::autoDetectByReceivedData);

        ASSERT_TRUE(m_acceptor->setNonBlockingMode(true));
        ASSERT_TRUE(m_acceptor->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_acceptor->listen());
        m_acceptor->acceptAsync(
            std::bind(&SslSocketVerifySslIsActuallyUsed::onAcceptCompletion, this, _1, _2));
    }

    void onAcceptCompletion(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
    {
        using namespace std::placeholders;

        ASSERT_EQ(SystemError::noError, sysErrorCode);
        ASSERT_TRUE(streamSocket->setNonBlockingMode(true));

        m_acceptor->acceptAsync(
            std::bind(&SslSocketVerifySslIsActuallyUsed::onAcceptCompletion, this, _1, _2));

        m_reflectorPoolInUse->add(std::move(streamSocket));
    }

    void prepareTestData()
    {
        m_testData.resize(nx::utils::random::number<int>(1024, 4096));
        std::generate(m_testData.data(), m_testData.data() + m_testData.size(), &rand);
    }
};

//-------------------------------------------------------------------------------------------------
// Asynchronous I/O

class SslSocketVerifySslIsActuallyUsedByAsyncIo:
    public SslSocketVerifySslIsActuallyUsed
{
public:
    SslSocketVerifySslIsActuallyUsedByAsyncIo()
    {
        switchToAsynchronousMode();
    }
};

TEST_F(SslSocketVerifySslIsActuallyUsedByAsyncIo, DISABLED_read_write)
{
    givenEstablishedConnection();
    whenSentRandomData();
    thenSameDataHasBeenReceivedInResponse();
}

//-------------------------------------------------------------------------------------------------
// Synchronous I/O

class SslSocketVerifySslIsActuallyUsedBySyncIo:
    public SslSocketVerifySslIsActuallyUsed
{
public:
    SslSocketVerifySslIsActuallyUsedBySyncIo()
    {
        switchToSynchronousMode();
    }
};

TEST_F(SslSocketVerifySslIsActuallyUsedBySyncIo, DISABLED_read_write)
{
    givenEstablishedConnection();
    whenSentRandomData();
    thenSameDataHasBeenReceivedInResponse();
}

//-------------------------------------------------------------------------------------------------
// Mixing sync & async mode.

class SslSocketSwitchIoMode:
    public SslSocketVerifySslIsActuallyUsed
{
protected:
    void exchangeDataInSyncMode()
    {
        switchToSynchronousMode();
        whenSentRandomData();
        thenSameDataHasBeenReceivedInResponse();
    }

    void changeSocketIoMode()
    {
        ASSERT_EQ(1U, reflectorPoolInUse()->count());
        auto socket = reflectorPoolInUse()->takeSocket();
        unusedReflectorPool()->add(std::move(socket));
    }

    void exchangeDataInAsyncMode()
    {
        switchToAsynchronousMode();
        whenSentRandomData();
        thenSameDataHasBeenReceivedInResponse();
    }
};

TEST_F(SslSocketSwitchIoMode, DISABLED_from_async_to_sync)
{
    givenEstablishedConnection();

    exchangeDataInAsyncMode();
    changeSocketIoMode();
    exchangeDataInSyncMode();
}

TEST_F(SslSocketSwitchIoMode, DISABLED_from_sync_to_async)
{
    givenEstablishedConnection();

    exchangeDataInSyncMode();
    changeSocketIoMode();
    exchangeDataInAsyncMode();
}

//-------------------------------------------------------------------------------------------------
// Common socket tests. These are not enough since they do not even check ssl is actually used.

#if 0

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketNotEncryptedConnection,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::autoDetectByReceivedData);
    },
    []() { return std::make_unique<TCPSocket>(AF_INET); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketEncryptedConnection,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::always);
    },
    []() { return std::make_unique<ssl::StreamSocket>(std::make_unique<TCPSocket>(AF_INET), false); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketNotEncryptedConnection2,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::autoDetectByReceivedData);
    },
    []() { return std::make_unique<ssl::StreamSocket>(std::make_unique<TCPSocket>(AF_INET), false); });

#endif

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
