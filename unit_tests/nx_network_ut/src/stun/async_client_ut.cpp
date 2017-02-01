#include <memory>

#include <gtest/gtest.h>

#include <nx/network/stun/async_client.h>
#include <nx/network/stun/async_client_user.h>
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
    StunClient()
    {
        m_stunClient = std::make_shared<nx::stun::AsyncClient>();
    }

    ~StunClient()
    {
        m_stunClient->pleaseStopSync();
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

    std::shared_ptr<nx::stun::AsyncClient> getStunClient()
    {
        return m_stunClient;
    }

    void givenRegularStunServer()
    {
        using namespace std::placeholders;

        m_dispatcher.registerRequestProcessor(
            kTestMethodNumber,
            std::bind(&StunClient::sendResponse, this, _1, _2));

        startServer();
    }

    void givenClientConnectedToServer()
    {
        if (!m_server)
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

        m_stunClient->connect(m_serverEndpoint, false);

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
        m_stunClient->setOnConnectionClosedHandler(
            [this](SystemError::ErrorCode closeReason)
            {
                m_connectionClosedPromise.set_value(closeReason);
            });
        m_stunClient->connect(
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
        sendRequest(m_stunClient.get());
    }

    template<typename AsyncClientType>
    void sendRequest(AsyncClientType* client)
    {
        using namespace std::placeholders;

        stun::Message request(stun::Header(
            stun::MessageClass::request,
            kTestMethodNumber));
        client->sendRequest(
            std::move(request),
            std::bind(&StunClient::storeRequestResult, this, _1, _2));
    }

    SystemError::ErrorCode getNextRequestResult()
    {
        return m_requestResult.pop();
    }

private:
    std::shared_ptr<nx::stun::AsyncClient> m_stunClient;
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

    void sendResponse(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        nx::stun::Message request)
    {
        nx::stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                request.header.method,
                request.header.transactionId));
        connection->sendMessage(std::move(response));
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
        nx::stun::Message request)
    {
        if (nx::utils::random::number<int>(0, 1) > 0)
        {
            connection->close();
            return;
        }

        nx::stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                request.header.method,
                request.header.transactionId));
        connection->sendMessage(std::move(response));
    }
};

TEST_F(FtStunClient, every_request_result_is_delivered)
{
    givenServerThatRandomlyClosesConnections();
    whenIssuedMultipleRequests();
    thenEveryRequestResultHasBeenReceived();
}

//-------------------------------------------------------------------------------------------------
// FtStunClientUser

class FtStunClientUser:
    public FtStunClient
{
protected:
    void assertIfResponseReportedAfterClientRemoval()
    {
        constexpr int kTestRunCount = 100;

        for (int i = 0; i < kTestRunCount; ++i)
        {
            std::atomic<bool> clientUserStopped(false);
            auto clientUser = std::make_unique<AsyncClientUser>(getStunClient());

            stun::Message request(stun::Header(
                stun::MessageClass::request,
                kTestMethodNumber));
            clientUser->sendRequest(
                std::move(request),
                [&clientUserStopped](SystemError::ErrorCode /*sysErrorCode*/, stun::Message /*response*/)
                {
                    ASSERT_FALSE(clientUserStopped.load());
                });

            std::this_thread::sleep_for(
                std::chrono::milliseconds(nx::utils::random::number<int>(0, 10)));

            nx::utils::promise<void> clientUserRemoved;
            clientUser->post(
                [&clientUser, &clientUserRemoved, &clientUserStopped]()
                {
                    clientUser->pleaseStopSync();
                    clientUserStopped = true;
                    clientUserRemoved.set_value();
                });
            clientUserRemoved.get_future().wait();
        }
    }
};

TEST_F(FtStunClientUser, correct_cancellation)
{
    using namespace std::placeholders;

    givenRegularStunServer();
    givenClientConnectedToServer();
    assertIfResponseReportedAfterClientRemoval();
}

} // namespace test
} // namespace stun
} // namespase nx
