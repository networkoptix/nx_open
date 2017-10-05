#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relaying/settings.h>

#include <http/target_peer_connector.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

namespace {

class StreamSocketThatCannotConnect:
    public nx::network::TCPSocket
{
public:
    virtual void connectAsync(
        const SocketAddress& /*addr*/,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> /*handler*/) override
    {
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class TargetPeerConnector:
    public ::testing::Test
{
public:
    TargetPeerConnector():
        m_peerName(nx::utils::generateRandomName(7).toStdString()),
        m_listeningPeerPool(m_listeningPeerPoolSettings)
    {
    }

    ~TargetPeerConnector()
    {
        if (m_targetPeerConnector)
            m_targetPeerConnector->pleaseStopSync();

        if (m_streamSocketFactoryBak)
            SocketFactory::setCreateStreamSocketFunc(std::move(*m_streamSocketFactoryBak));
    }

protected:
    void givenDirectlyAccessibleServer()
    {
        m_targetEndpoint = m_testServer.serverAddress();
    }

    void givenRegisteredListeningPeer()
    {
        auto connection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(connection->connect(m_testServer.serverAddress()));
        ASSERT_TRUE(connection->setNonBlockingMode(true));
        m_serverConnection = connection.get();

        m_listeningPeerPool.addConnection(m_peerName, std::move(connection));

        m_targetEndpoint = SocketAddress(m_peerName.c_str(), 0);
    }

    void whenConnect()
    {
        using namespace std::placeholders;

        m_targetPeerConnector = std::make_unique<gateway::TargetPeerConnector>(
            &m_listeningPeerPool,
            m_targetEndpoint);
        if (m_connectTimeout)
            m_targetPeerConnector->setTimeout(*m_connectTimeout);

        m_targetPeerConnector->connectAsync(
            std::bind(&TargetPeerConnector::saveConnectionResult, this, _1, _2));
    }

    void thenConnectionIsEstablished()
    {
        m_prevResult = m_connectResultQueue.pop();
        ASSERT_EQ(SystemError::noError, m_prevResult.systemErrorCode);
        ASSERT_NE(nullptr, m_prevResult.connection);
    }

    void thenDirectConnectionIsEstablished()
    {
        // TODO Check that connection is actually established.
        thenConnectionIsEstablished();
    }

    void thenTimeoutIsReported()
    {
        m_prevResult = m_connectResultQueue.pop();
        ASSERT_EQ(SystemError::timedOut, m_prevResult.systemErrorCode);
        ASSERT_EQ(nullptr, m_prevResult.connection);
    }

    void thenConnectionIsTakenFromListeningPeer()
    {
        thenConnectionIsEstablished();
        ASSERT_EQ(m_serverConnection, m_prevResult.connection.get());
    }

    void andTimeoutHasBeenSetOnUnderlyingSocket()
    {
        unsigned int sendTimeoutMillis = 0;
        ASSERT_TRUE(m_prevResult.connection->getSendTimeout(&sendTimeoutMillis));
        ASSERT_EQ(m_connectTimeout->count(), sendTimeoutMillis);
    }

    void switchToStreamSocketThatCannotConnect()
    {
        using namespace std::placeholders;

        m_streamSocketFactoryBak = SocketFactory::setCreateStreamSocketFunc(
            std::bind(&TargetPeerConnector::createStreamSocket, this, _1, _2));
    }

    void enableConnectTimeout(std::chrono::milliseconds timeout)
    {
        m_connectTimeout = timeout;
    }

private:
    struct ConnectResult
    {
        SystemError::ErrorCode systemErrorCode;
        std::unique_ptr<AbstractStreamSocket> connection;
    };

    TestHttpServer m_testServer;
    std::string m_peerName;
    std::unique_ptr<gateway::TargetPeerConnector> m_targetPeerConnector;
    nx::utils::SyncQueue<ConnectResult> m_connectResultQueue;
    SocketAddress m_targetEndpoint;
    boost::optional<SocketFactory::CreateStreamSocketFuncType> m_streamSocketFactoryBak;
    boost::optional<std::chrono::milliseconds> m_connectTimeout;
    relaying::Settings m_listeningPeerPoolSettings;
    relaying::ListeningPeerPool m_listeningPeerPool;
    ConnectResult m_prevResult;
    AbstractStreamSocket* m_serverConnection = nullptr;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_testServer.bindAndListen());
    }

    void saveConnectionResult(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
        m_connectResultQueue.push({systemErrorCode, std::move(connection)});
    }

    std::unique_ptr<AbstractStreamSocket> createStreamSocket(
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/)
    {
        return std::make_unique<StreamSocketThatCannotConnect>();
    }
};

TEST_F(TargetPeerConnector, regular_connection_is_established)
{
    givenDirectlyAccessibleServer();
    whenConnect();
    thenDirectConnectionIsEstablished();
}

TEST_F(TargetPeerConnector, timeout)
{
    switchToStreamSocketThatCannotConnect();
    enableConnectTimeout(std::chrono::milliseconds(1));
    
    givenDirectlyAccessibleServer();
    whenConnect();
    thenTimeoutIsReported();
}

// That's a requirement of cloud connect "reverse" method.
TEST_F(TargetPeerConnector, passes_timeout_to_underlying_socket)
{
    enableConnectTimeout(std::chrono::hours(1));

    givenDirectlyAccessibleServer();
    whenConnect();
    thenDirectConnectionIsEstablished();
    andTimeoutHasBeenSetOnUnderlyingSocket();
}

//TEST_F(TargetPeerConnector, reports_regular_connection_error)

TEST_F(TargetPeerConnector, takes_connection_from_listening_peer_pool)
{
    givenRegisteredListeningPeer();
    whenConnect();
    thenConnectionIsTakenFromListeningPeer();
}

//TEST_F(TargetPeerConnector, timeout_reported_if_listening_peer_pool_does_not_answer)

//TEST_F(TargetPeerConnector, listening_peer_pool_is_checked_first)

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
