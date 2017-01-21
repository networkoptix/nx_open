#include <memory>

#include <gtest/gtest.h>

#include <nx/network/stun/async_client.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/sync_queue.h>

namespace nx {
namespace stun {
namespace test {

static constexpr int kTestMethodNumber = stun::MethodType::userMethod + 1;

class StunClient:
    public ::testing::Test
{
public:
    ~StunClient()
    {
        m_stunClient.pleaseStopSync();
        if (m_server)
        {
            m_server->pleaseStopSync();
            m_server.reset();
        }
    }

protected:
    nx::stun::MessageDispatcher& dispatcher()
    {
        return m_dispatcher;
    }

    void givenClientConnectedToServer()
    {
        startServer();

        connectClientToTheServer();
    }

    void givenServerThatBreaksConnectionAfterReceivingRequest()
    {
        using namespace std::placeholders;

        m_dispatcher.registerRequestProcessor(
            kTestMethodNumber,
            std::bind(&StunClient::closeConnection, this, _1, _2));

        startServer();
    }

    void whenServerTerminatedAbruptly()
    {
        m_server->pleaseStopSync();
        m_server.reset();
    }

    void whenClientSendsRequestToTheServer()
    {
        using namespace std::placeholders;

        m_stunClient.connect(m_serverEndpoint, false);

        sendRequest();
    }

    void verifyClientProcessedConnectionCloseProperly()
    {
        ASSERT_NE(SystemError::noError, m_connectionClosedPromise.get_future().get());
    }

    void thenErrorResultIsReported()
    {
        ASSERT_NE(SystemError::noError, getNextRequestResult());
    }

    void startServer()
    {
        m_server = std::make_unique<stun::SocketServer>(&m_dispatcher, false);

        ASSERT_TRUE(m_server->bind(SocketAddress::anyPrivateAddress))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(m_server->listen()) << SystemError::getLastOSErrorText().toStdString();

        m_serverEndpoint = m_server->address();
    }

    void connectClientToTheServer()
    {
        nx::utils::promise<SystemError::ErrorCode> connectedPromise;
        m_stunClient.setOnConnectionClosedHandler(
            [this](SystemError::ErrorCode closeReason)
            {
                m_connectionClosedPromise.set_value(closeReason);
            });
        m_stunClient.connect(
            m_serverEndpoint,
            false,
            [&connectedPromise](SystemError::ErrorCode sysErrorCode)
            {
                connectedPromise.set_value(sysErrorCode);
            });
        ASSERT_EQ(SystemError::noError, connectedPromise.get_future().get());
    }

    void sendRequest()
    {
        using namespace std::placeholders;

        stun::Message request(stun::Header(
            stun::MessageClass::request,
            kTestMethodNumber));
        m_stunClient.sendRequest(
            std::move(request),
            std::bind(&StunClient::storeRequestResult, this, _1, _2));
    }

    SystemError::ErrorCode getNextRequestResult()
    {
        return m_requestResult.pop();
    }

private:
    nx::stun::AsyncClient m_stunClient;
    nx::utils::promise<SystemError::ErrorCode> m_connectionClosedPromise;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_requestResult;
    SocketAddress m_serverEndpoint;
    nx::stun::MessageDispatcher m_dispatcher;
    std::unique_ptr<stun::SocketServer> m_server;

    void closeConnection(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        nx::stun::Message /*message*/)
    {
        connection->close();
    }

    void storeRequestResult(SystemError::ErrorCode sysErrorCode, stun::Message /*response*/)
    {
        m_requestResult.push(sysErrorCode);
    }
};

TEST_F(StunClient, proper_cancellation_when_connection_terminated_by_remote_side)
{
    givenClientConnectedToServer();
    whenServerTerminatedAbruptly();
    verifyClientProcessedConnectionCloseProperly();
}

TEST_F(StunClient, request_result_correctly_reported_on_connection_break)
{
    givenServerThatBreaksConnectionAfterReceivingRequest();
    whenClientSendsRequestToTheServer();
    thenErrorResultIsReported();
}

class FtStunClient:
    public StunClient
{
public:
    static constexpr int kNumberOfRequestsToSend = 1024;

protected:
    void givenServerThatRandomlyClosesConnections()
    {
        using namespace std::placeholders;

        dispatcher().registerRequestProcessor(
            kTestMethodNumber,
            std::bind(&FtStunClient::randomlyCloseConnection, this, _1, _2));

        startServer();
    }

    void whenIssuedMultipleRequests()
    {
        connectClientToTheServer();

        for (int i = 0; i < kNumberOfRequestsToSend; ++i)
            sendRequest();
    }

    void thenEveryRequestResultHasBeenReceived()
    {
        for (int i = 0; i < kNumberOfRequestsToSend; ++i)
            getNextRequestResult();
    }

private:
    void randomlyCloseConnection(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        nx::stun::Message /*message*/)
    {
        if (nx::utils::random::number<int>(0, 1) > 0)
            connection->close();
    }
};

TEST_F(FtStunClient, every_request_result_is_delivered)
{
    givenServerThatRandomlyClosesConnections();
    whenIssuedMultipleRequests();
    thenEveryRequestResultHasBeenReceived();
}

} // namespace test
} // namespace hpm
} // namespase nx
