#include <gtest/gtest.h>

#include <nx/network/aio/async_channel_reflector.h>
#include <nx/network/ssl/ssl_stream_server_socket.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/stream_socket_acceptance_tests.h>
#include <nx/utils/thread/mutex.h>

#include "sync_ssl_socket.h"

namespace nx {
namespace network {

namespace test {

namespace {

class EnforcedSslOverTcpServerSocket:
    public ssl::StreamServerSocket
{
    using base_type = ssl::StreamServerSocket;

public:
    EnforcedSslOverTcpServerSocket(int ipVersion = AF_INET):
        base_type(
            std::make_unique<TCPServerSocket>(ipVersion),
            ssl::EncryptionUse::always)
    {
    }
};

class AutoDetectedSslOverTcpServerSocket:
    public ssl::StreamServerSocket
{
    using base_type = ssl::StreamServerSocket;

public:
    AutoDetectedSslOverTcpServerSocket(int ipVersion = AF_INET):
        base_type(
            std::make_unique<TCPServerSocket>(ipVersion),
            ssl::EncryptionUse::autoDetectByReceivedData)
    {
    }
};

class SslOverTcpStreamSocket:
    public ssl::StreamSocket
{
    using base_type = ssl::StreamSocket;

public:
    SslOverTcpStreamSocket(int ipVersion = AF_INET):
        base_type(std::make_unique<TCPSocket>(ipVersion), false)
    {
    }
};

struct SslSocketBothEndsEncryptedTypeSet
{
    using ClientSocket = SslOverTcpStreamSocket;
    using ServerSocket = EnforcedSslOverTcpServerSocket;
};

struct SslSocketBothEndsEncryptedAutoDetectingServerTypeSet
{
    using ClientSocket = SslOverTcpStreamSocket;
    using ServerSocket = AutoDetectedSslOverTcpServerSocket;
};

struct SslSocketClientNotEncryptedTypeSet
{
    using ClientSocket = TCPSocket;
    using ServerSocket = AutoDetectedSslOverTcpServerSocket;
};

} // namespace

INSTANTIATE_TYPED_TEST_CASE_P(
    SslSocketBothEndsEncrypted,
    StreamSocketAcceptance,
    SslSocketBothEndsEncryptedTypeSet);

INSTANTIATE_TYPED_TEST_CASE_P(
    SslSocketBothEndsEncryptedAutoDetectingServer,
    StreamSocketAcceptance,
    SslSocketBothEndsEncryptedAutoDetectingServerTypeSet);

INSTANTIATE_TYPED_TEST_CASE_P(
    SslSocketClientNotEncrypted,
    StreamSocketAcceptance,
    SslSocketClientNotEncryptedTypeSet);

} // namespace test

//-------------------------------------------------------------------------------------------------

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

//-------------------------------------------------------------------------------------------------

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

//-------------------------------------------------------------------------------------------------

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

// Verifies by using ClientSyncSslSocket class on one side.

TEST_F(SslSocketVerifySslIsActuallyUsed, ssl_used_by_async_io)
{
    switchToAsynchronousMode();

    givenEstablishedConnection();
    whenSentRandomData();
    thenSameDataHasBeenReceivedInResponse();
}

TEST_F(SslSocketVerifySslIsActuallyUsed, ssl_used_by_sync_io)
{
    switchToSynchronousMode();

    givenEstablishedConnection();
    whenSentRandomData();
    thenSameDataHasBeenReceivedInResponse();
}

//-------------------------------------------------------------------------------------------------

class SslSocketSpecific:
    public network::test::StreamSocketAcceptance<
        network::test::SslSocketBothEndsEncryptedAutoDetectingServerTypeSet>
{
    using base_type = network::test::StreamSocketAcceptance<
        network::test::SslSocketBothEndsEncryptedAutoDetectingServerTypeSet>;

protected:
    void givenSocketTimedOutOnSendAsync()
    {
        givenAcceptingServerSocket();
        givenConnectedSocket();
        setClientSocketSendTimeout(std::chrono::milliseconds(1));

        whenClientSendsRandomDataAsyncNonStop();

        thenClientSendTimesOutEventually();
    }

    void givenSocketTimedOutOnSend()
    {
        givenAcceptingServerSocket();
        givenConnectedSocket();
        setClientSocketSendTimeout(std::chrono::milliseconds(1));

        const auto randomData = nx::utils::generateRandomName(64*1024);
        for (;;)
        {
            int bytesSent =
                connection()->send(randomData.constData(), randomData.size());
            if (bytesSent >= 0)
                continue;

            ASSERT_EQ(SystemError::timedOut, SystemError::getLastOSErrorCode());
            break;
        }
    }

    void givenNotEncryptedConnection()
    {
        m_notEncryptedConnection = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(m_notEncryptedConnection->connect(serverEndpoint(), kNoTimeout));

        nx::Buffer testData("Hello, world");
        ASSERT_EQ(
            testData.size(),
            m_notEncryptedConnection->send(testData.constData(), testData.size()));
    }

    void assertClientConnectionReportsEncryptionEnabled()
    {
        ASSERT_TRUE(connection()->isEncryptionEnabled());
    }

    void assertServerConnectionReportsEncryptionEnabled()
    {
        thenConnectionHasBeenAccepted();

        ASSERT_TRUE(
            static_cast<AbstractEncryptedStreamSocket*>(lastAcceptedSocket())
                ->isEncryptionEnabled());
    }

    void assertServerConnectionReportsEncryptionDisabled()
    {
        thenConnectionHasBeenAccepted();

        ASSERT_FALSE(
            static_cast<AbstractEncryptedStreamSocket*>(lastAcceptedSocket())
                ->isEncryptionEnabled());
    }

private:
    std::unique_ptr<TCPSocket> m_notEncryptedConnection;
};

TEST_F(SslSocketSpecific, socket_becomes_unusable_after_async_send_timeout)
{
    givenSocketTimedOutOnSendAsync();
    whenClientSendsPingAsync();
    thenSendFailedUnrecoverableError();
}

TEST_F(SslSocketSpecific, socket_becomes_unusable_after_sync_send_timeout)
{
    givenSocketTimedOutOnSend();
    whenClientSendsPing();
    thenSendFailedUnrecoverableError();
}

TEST_F(SslSocketSpecific, enabled_encryption_is_reported)
{
    givenAcceptingServerSocket();
    givenConnectedSocket();

    assertClientConnectionReportsEncryptionEnabled();
    assertServerConnectionReportsEncryptionEnabled();
}

TEST_F(SslSocketSpecific, disabled_encryption_is_reported)
{
    givenAcceptingServerSocket();
    givenNotEncryptedConnection();

    assertServerConnectionReportsEncryptionDisabled();
}

// TEST_F(SslSocketSpecific, timeout_during_handshake_is_handled_properly)

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

TEST_F(SslSocketSwitchIoMode, from_async_to_sync)
{
    givenEstablishedConnection();

    exchangeDataInAsyncMode();
    changeSocketIoMode();
    exchangeDataInSyncMode();
}

TEST_F(SslSocketSwitchIoMode, from_sync_to_async)
{
    givenEstablishedConnection();

    exchangeDataInSyncMode();
    changeSocketIoMode();
    exchangeDataInAsyncMode();
}

//-------------------------------------------------------------------------------------------------
// Common socket tests. These are not enough since they do not even check ssl is actually used.

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketEncryptedConnection,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::always);
    },
    []()
    {
        return std::make_unique<ssl::StreamSocket>(
            std::make_unique<TCPSocket>(AF_INET), false);
    });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketEncryptedConnectionAutoDetected,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::autoDetectByReceivedData);
    },
    []()
    {
        return std::make_unique<ssl::StreamSocket>(
            std::make_unique<TCPSocket>(AF_INET), false);
    });

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
