#include <memory>

#include <gtest/gtest.h>

#include <nx/network/stun/async_client.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace stun {
namespace test {

static constexpr int testMethod = stun::MethodType::userMethod + 1;

class StunClient:
    public ::testing::Test
{
public:
    ~StunClient()
    {
        m_stunClient.pleaseStopSync();
        if (m_server)
        {
            m_server->pleaseStop();
            m_server.reset();
        }
    }

protected:
    void givenClientConnectedToServer()
    {
        startServer();

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

    void givenServerThatBreaksConnectionAfterReceivingRequest()
    {
        using namespace std::placeholders;

        m_dispatcher.registerRequestProcessor(
            testMethod,
            std::bind(&StunClient::closeConnection, this, _1, _2));

        startServer();
    }

    void whenServerTerminatedAbruptly()
    {
        m_server->pleaseStop();
        m_server.reset();
    }

    void whenClientSendsRequestToTheServer()
    {
        using namespace std::placeholders;

        stun::Message request(stun::Header(
            stun::MessageClass::request,
            testMethod));
        m_stunClient.connect(m_serverEndpoint, false);
        m_stunClient.sendRequest(
            std::move(request),
            std::bind(&StunClient::storeRequestResult, this, _1, _2));
    }

    void verifyClientProcessedConnectionCloseProperly()
    {
        ASSERT_NE(SystemError::noError, m_connectionClosedPromise.get_future().get());
    }

    void thenErrorResultIsReported()
    {
        ASSERT_NE(SystemError::noError, m_requestResult.get_future().get());
    }

private:
    nx::stun::AsyncClient m_stunClient;
    nx::utils::promise<SystemError::ErrorCode> m_connectionClosedPromise;
    nx::utils::promise<SystemError::ErrorCode> m_requestResult;
    SocketAddress m_serverEndpoint;
    nx::stun::MessageDispatcher m_dispatcher;
    std::unique_ptr<stun::SocketServer> m_server;

    void startServer()
    {
        m_server = std::make_unique<stun::SocketServer>(&m_dispatcher, false);

        ASSERT_TRUE(m_server->bind(SocketAddress::anyPrivateAddress))
            << SystemError::getLastOSErrorText().toStdString();
        ASSERT_TRUE(m_server->listen()) << SystemError::getLastOSErrorText().toStdString();

        m_serverEndpoint = m_server->address();
    }

    void closeConnection(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        nx::stun::Message /*message*/)
    {
        connection->close();
    }

    void storeRequestResult(SystemError::ErrorCode sysErrorCode, stun::Message /*response*/)
    {
        m_requestResult.set_value(sysErrorCode);
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

} // namespace test
} // namespace hpm
} // namespase nx
