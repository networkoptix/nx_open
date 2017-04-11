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

//-------------------------------------------------------------------------------------------------
// Test fixture

/**
 * These tests use SyncSslSocket on one side to guarantee that ssl is actually implied in test.
 */
class SslSocketServerSide:
    public ::testing::Test
{
public:
    SslSocketServerSide():
        m_clientSocket(std::make_unique<TCPSocket>(AF_INET)),
        m_terminated(false)
    {
        startAcceptor();
    }

    ~SslSocketServerSide()
    {
        if (m_acceptor)
            m_acceptor->pleaseStopSync();

        decltype (m_acceptedConnections) acceptedConnections;
        {
            QnMutexLocker lock(&m_mutex);
            m_terminated = true;
            acceptedConnections.swap(m_acceptedConnections);
        }

        for (auto& connection: acceptedConnections)
            connection.reflector->pleaseStopSync();
    }

protected:
    void whenSentRandomData()
    {
        prepareTestData();

        ASSERT_TRUE(m_clientSocket.connect(m_acceptor->getLocalAddress()));
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

private:
    struct AcceptedConnectionContext
    {
        std::unique_ptr<aio::BasicPollable> reflector;
    };

    //struct AcceptResult
    //{
    //    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    //    std::unique_ptr<AbstractStreamSocket> streamSocket;
    //};

    std::unique_ptr<ssl::StreamServerSocket> m_acceptor;
    std::list<AcceptedConnectionContext> m_acceptedConnections;
    //utils::SyncQueue<AcceptResult> m_acceptResults;
    ClientSyncSslSocket m_clientSocket;
    nx::Buffer m_testData;
    mutable QnMutex m_mutex;
    bool m_terminated;

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
            std::bind(&SslSocketServerSide::onAcceptCompletion, this, _1, _2));
    }

    void onAcceptCompletion(
        SystemError::ErrorCode sysErrorCode,
        AbstractStreamSocket* streamSocket)
    {
        using namespace std::placeholders;

        ASSERT_EQ(SystemError::noError, sysErrorCode);
        ASSERT_TRUE(streamSocket->setNonBlockingMode(true));

        m_acceptor->acceptAsync(
            std::bind(&SslSocketServerSide::onAcceptCompletion, this, _1, _2));

        QnMutexLocker lock(&m_mutex);
        m_acceptedConnections.push_back(AcceptedConnectionContext());
        auto reflector = aio::makeAsyncChannelReflector(
            std::unique_ptr<AbstractStreamSocket>(streamSocket));
        reflector->start(
            std::bind(&SslSocketServerSide::onReflectorDone, this,
                --m_acceptedConnections.end(), _1));
        m_acceptedConnections.back().reflector = std::move(reflector);
    }

    void onReflectorDone(
        std::list<AcceptedConnectionContext>::iterator reflectorIter,
        SystemError::ErrorCode /*sysErrorCode*/)
    {
        QnMutexLocker lock(&m_mutex);
        if (!m_terminated)
            m_acceptedConnections.erase(reflectorIter);
    }

    void prepareTestData()
    {
        m_testData.resize(nx::utils::random::number<int>(1024, 4096));
        std::generate(m_testData.data(), m_testData.data() + m_testData.size(), &rand);
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases

TEST_F(SslSocketServerSide, DISABLED_send_receive_data)
{
    whenSentRandomData();
    thenSameDataHasBeenReceivedInResponse();
}

//-------------------------------------------------------------------------------------------------
// Common socket tests. These are not enough since they do not even check ssl is actually used.

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

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
