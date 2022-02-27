// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/vms_gateway/http/target_peer_connector.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

namespace {

class StreamSocketThatCannotConnect:
    public nx::network::TCPSocket
{
public:
    enum class ConnectFailureType
    {
        doNotReportResult,
        reportError,
    };

    StreamSocketThatCannotConnect(ConnectFailureType connectFailureType):
        m_connectFailureType(connectFailureType)
    {
    }

    virtual void connectAsync(
        const nx::network::SocketAddress& /*addr*/,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        if (m_connectFailureType == ConnectFailureType::reportError)
            post([handler = std::move(handler)]() { handler(SystemError::connectionRefused); });
    }

private:
    ConnectFailureType m_connectFailureType;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class TargetPeerConnector:
    public ::testing::Test
{
public:
    TargetPeerConnector():
        m_peerName(nx::utils::generateRandomName(7))
    {
    }

    ~TargetPeerConnector()
    {
        if (m_targetPeerConnector)
            m_targetPeerConnector->pleaseStopSync();

        if (m_streamSocketFactoryBak)
            nx::network::SocketFactory::setCreateStreamSocketFunc(std::move(*m_streamSocketFactoryBak));
    }

protected:
    void givenDirectlyAccessibleServer()
    {
        m_targetEndpoint = m_testServer.serverAddress();
    }

    void givenRegisteredListeningPeer()
    {
        auto connection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(connection->connect(m_testServer.serverAddress(), nx::network::kNoTimeout));
        ASSERT_TRUE(connection->setNonBlockingMode(true));
        m_serverConnection = connection.get();

        m_targetEndpoint = nx::network::SocketAddress(m_peerName, 0);
    }

    void whenConnect()
    {
        using namespace std::placeholders;

        m_targetPeerConnector = std::make_unique<gateway::TargetPeerConnector>();
        if (m_connectTimeout)
            m_targetPeerConnector->setTimeout(*m_connectTimeout);

        m_targetPeerConnector->connectAsync(
            m_targetEndpoint,
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

    void thenConnectFailed()
    {
        m_prevResult = m_connectResultQueue.pop();
        ASSERT_NE(SystemError::noError, m_prevResult.systemErrorCode);
        ASSERT_EQ(nullptr, m_prevResult.connection);
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

    void switchToStreamSocketThatNeverReportsConnectResult()
    {
        m_streamSocketFactoryBak = nx::network::SocketFactory::setCreateStreamSocketFunc(
            [](auto&&...) -> std::unique_ptr<nx::network::AbstractStreamSocket>
            {
                return std::make_unique<StreamSocketThatCannotConnect>(
                    StreamSocketThatCannotConnect::ConnectFailureType::doNotReportResult);
            });
    }

    void switchToStreamSocketThatAlwaysFailsToConnect()
    {
        m_streamSocketFactoryBak = nx::network::SocketFactory::setCreateStreamSocketFunc(
            [](auto&&...) -> std::unique_ptr<nx::network::AbstractStreamSocket>
            {
                return std::make_unique<StreamSocketThatCannotConnect>(
                    StreamSocketThatCannotConnect::ConnectFailureType::reportError);
            });
    }

    void enableConnectTimeout(std::chrono::milliseconds timeout)
    {
        m_connectTimeout = timeout;
    }

private:
    struct ConnectResult
    {
        SystemError::ErrorCode systemErrorCode;
        std::unique_ptr<nx::network::AbstractStreamSocket> connection;
    };

    nx::network::http::TestHttpServer m_testServer;
    std::string m_peerName;
    std::unique_ptr<gateway::TargetPeerConnector> m_targetPeerConnector;
    nx::utils::SyncQueue<ConnectResult> m_connectResultQueue;
    nx::network::SocketAddress m_targetEndpoint;
    std::optional<nx::network::SocketFactory::CreateStreamSocketFuncType> m_streamSocketFactoryBak;
    std::optional<std::chrono::milliseconds> m_connectTimeout;
    ConnectResult m_prevResult;
    nx::network::AbstractStreamSocket* m_serverConnection = nullptr;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_testServer.bindAndListen());
    }

    void saveConnectionResult(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<nx::network::AbstractStreamSocket> connection)
    {
        m_connectResultQueue.push({systemErrorCode, std::move(connection)});
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
    switchToStreamSocketThatNeverReportsConnectResult();
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

TEST_F(TargetPeerConnector, reports_regular_connection_error)
{
    switchToStreamSocketThatAlwaysFailsToConnect();

    givenDirectlyAccessibleServer();
    whenConnect();
    thenConnectFailed();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
