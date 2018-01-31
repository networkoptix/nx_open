#pragma once

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace stun {
namespace test {

class AbstractStunServer
{
public:
    virtual ~AbstractStunServer() = default;

    virtual bool bind(const SocketAddress&) = 0;
    virtual bool listen() = 0;
    virtual QUrl getServerUrl() const = 0;
    virtual void sendIndicationThroughEveryConnection(nx::stun::Message) = 0;
    virtual nx::stun::MessageDispatcher& dispatcher() = 0;
    virtual std::size_t connectionCount() const = 0;
};

class BasicStunAsyncClientAcceptanceTest:
    public ::testing::Test
{
public:
    ~BasicStunAsyncClientAcceptanceTest();

protected:
    void setSingleShotUnconnectableSocketFactory();

private:
    boost::optional<SocketFactory::CreateStreamSocketFuncType> m_streamSocketFactoryBak;
    QnMutex m_mutex;

    std::unique_ptr<AbstractStreamSocket> createUnconnectableStreamSocket(
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/);
};

/**
 * @param AsyncClientTestTypes It is a struct with following nested types:
 * - ClientType Type that inherits nx::stun::AbstractAsyncClient.
 * - ServerType Class that implements AbstractStunServer interface.
 */
template<typename AsyncClientTestTypes>
class StunAsyncClientAcceptanceTest:
    public BasicStunAsyncClientAcceptanceTest
{
public:
    ~StunAsyncClientAcceptanceTest()
    {
        m_client.pleaseStopSync();
    }

protected:
    void subscribeToEveryIndication()
    {
        m_indictionMethodToSubscribeTo = nx::stun::kEveryIndicationMethod;
    }

    template<typename Func>
    void doInClientAioThread(Func func)
    {
        nx::utils::promise<void> done;
        m_client.post(
            [&func, &done]()
            {
                func();
                done.set_value();
            });
        done.get_future().wait();
    }

    void givenClientWithIndicationHandler()
    {
        ASSERT_TRUE(addIndicationHandler());
    }

    void givenConnectedClient()
    {
        initializeClient();
        thenClientConnected();
    }

    void givenReconnectedClient()
    {
        givenConnectedClient();
        whenRestartServer();
        thenClientReportsConnectionClosure();
        thenClientReconnects();
    }

    void givenBrokenServer()
    {
        startServer();
        whenStopServer();
    }

    void givenDisconnectedClient()
    {
        initializeClient();
    }

    void givenClientFailedToConnect()
    {
        setSingleShotUnconnectableSocketFactory();

        initializeClient();
        ASSERT_NE(SystemError::noError, m_connectResults.pop());
    }

    void whenRemoveHandler()
    {
        nx::utils::promise<void> done;
        m_client.cancelHandlers(this, [&done]() { done.set_value(); });
        done.get_future().wait();
    }

    void whenStopServer()
    {
        m_server.reset();
    }

    void whenRestartServer()
    {
        m_server.reset();

        startServer();
    }

    void whenIssueRequest()
    {
        using namespace std::placeholders;

        stun::Message request(stun::Header(
            stun::MessageClass::request,
            m_testMethodNumber));
        m_client.sendRequest(
            std::move(request),
            std::bind(&StunAsyncClientAcceptanceTest::storeRequestResult, this, _1, _2));
    }

    void whenForciblyCloseClientConnection()
    {
        m_client.closeConnection(SystemError::connectionReset);
    }

    void whenServerSendsIndication()
    {
        stun::Message indication(stun::Header(
            stun::MessageClass::indication,
            m_testMethodNumber));
        m_server->sendIndicationThroughEveryConnection(std::move(indication));
    }

    void whenServerRestartedOnAnotherEndpoint()
    {
        m_serverEndpoint = SocketAddress::anyPrivateAddress;
        decltype(m_server) oldServer;
        oldServer.swap(m_server);
        startServer();
    }

    void whenConnectToServer()
    {
        m_client.connect(
            m_serverUrl,
            [this](SystemError::ErrorCode systemErrorCode)
            {
                m_connectResults.push(systemErrorCode);
            });
    }

    void whenStartAnotherServer()
    {
        decltype(m_server) oldServer;
        oldServer.swap(m_server);
        m_oldServers.push_back(std::move(oldServer));

        m_serverEndpoint = SocketAddress::anyPrivateAddress;
        startServer();
    }

    void whenDisconnectClient()
    {
        m_client.closeConnection(SystemError::connectionReset);
    }

    void whenConnectToNewServer()
    {
        whenConnectToServer();
    }

    void thenClientConnected()
    {
        ASSERT_EQ(SystemError::noError, m_connectResults.pop());
        waitForServerToHaveAtLeastOneConnection();
    }

    void thenConnectFailed()
    {
        ASSERT_NE(SystemError::noError, m_connectResults.pop());
    }

    void thenSameHandlerCannotBeAdded()
    {
        ASSERT_FALSE(addIndicationHandler());
    }

    void thenSameHandlerCanBeAddedAgain()
    {
        ASSERT_TRUE(addIndicationHandler());
    }

    void thenClientReconnects()
    {
        m_reconnectEvents.pop();

        waitForServerToHaveAtLeastOneConnection();
    }

    void thenSuccessResponseIsReceived()
    {
        m_prevRequestResult = m_requestResult.pop();

        ASSERT_EQ(SystemError::noError, m_prevRequestResult.sysErrorCode);
        ASSERT_EQ(
            stun::MessageClass::successResponse,
            m_prevRequestResult.response.header.messageClass);
    }

    void thenResponseIsReceived()
    {
        m_prevRequestResult = m_requestResult.pop();
    }

    void thenRequestFailureIsReported()
    {
        m_prevRequestResult = m_requestResult.pop();
        ASSERT_NE(SystemError::noError, m_prevRequestResult.sysErrorCode);
    }

    void thenClientIsAbleToPerformRequests()
    {
        whenIssueRequest();
        thenSuccessResponseIsReceived();
    }

    void thenClientReceivesIndication()
    {
        m_indicationsReceived.pop();
    }

    void thenClientReportsConnectionClosure()
    {
        m_connectionClosedEventsReceived.pop();
    }

    void thenClientDoesNotReportConnectionClosure()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        ASSERT_TRUE(m_connectionClosedEventsReceived.isEmpty());
    }

    void andReconnectHandlerIsInvoked()
    {
        m_reconnectEvents.pop();
    }

    typename AsyncClientTestTypes::ClientType& client()
    {
        return m_client;
    }

private:
    struct RequestResult
    {
        SystemError::ErrorCode sysErrorCode = SystemError::noError;
        stun::Message response;

        RequestResult() = default;
        RequestResult(
            SystemError::ErrorCode sysErrorCode,
            stun::Message response)
            :
            sysErrorCode(sysErrorCode),
            response(std::move(response))
        {
        }
    };

    typename AsyncClientTestTypes::ClientType m_client;
    std::unique_ptr<typename AsyncClientTestTypes::ServerType> m_server;
    std::vector<std::unique_ptr<typename AsyncClientTestTypes::ServerType>> m_oldServers;
    SocketAddress m_serverEndpoint = SocketAddress::anyPrivateAddress;
    QUrl m_serverUrl;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectResults;
    nx::utils::SyncQueue<int /*dummy*/> m_reconnectEvents;
    nx::utils::SyncQueue<RequestResult> m_requestResult;
    nx::utils::SyncQueue<nx::stun::Message> m_indicationsReceived;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionClosedEventsReceived;
    RequestResult m_prevRequestResult;
    const int m_testMethodNumber = stun::MethodType::userMethod + 1;
    int m_indictionMethodToSubscribeTo = stun::MethodType::userMethod + 1;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        startServer();

        m_client.addOnReconnectedHandler(
            std::bind(&StunAsyncClientAcceptanceTest::onReconnected, this));
        m_client.setOnConnectionClosedHandler(
            std::bind(&StunAsyncClientAcceptanceTest::saveConnectionClosedEvent, this, _1));
    }

    void startServer()
    {
        using namespace std::placeholders;

        m_server = std::make_unique<typename AsyncClientTestTypes::ServerType>();

        m_server->dispatcher().registerRequestProcessor(
            m_testMethodNumber,
            std::bind(&StunAsyncClientAcceptanceTest::sendResponse, this, _1, _2));

        ASSERT_TRUE(m_server->bind(m_serverEndpoint));
        m_serverEndpoint = nx::network::url::getEndpoint(m_server->getServerUrl());
        m_serverUrl = m_server->getServerUrl();
        ASSERT_TRUE(m_server->listen());
    }

    void initializeClient()
    {
        using namespace std::placeholders;

        ASSERT_TRUE(m_client.setIndicationHandler(
            m_indictionMethodToSubscribeTo,
            std::bind(&StunAsyncClientAcceptanceTest::saveIndication, this, _1),
            this));

        m_client.connect(
            m_serverUrl,
            [this](SystemError::ErrorCode systemErrorCode)
            {
                m_connectResults.push(systemErrorCode);
            });
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

    void storeRequestResult(
        SystemError::ErrorCode sysErrorCode,
        stun::Message response)
    {
        m_requestResult.push(RequestResult(sysErrorCode, std::move(response)));
    }

    bool addIndicationHandler()
    {
        return m_client.setIndicationHandler(
            m_testMethodNumber + 1,
            [](nx::stun::Message) {},
            this);
    }

    void onReconnected()
    {
        m_reconnectEvents.push(0);
    }

    void saveConnectionClosedEvent(SystemError::ErrorCode reason)
    {
        m_connectionClosedEventsReceived.push(reason);
    }

    void saveIndication(nx::stun::Message indication)
    {
        m_indicationsReceived.push(std::move(indication));
    }

    void waitForServerToHaveAtLeastOneConnection()
    {
        // Waiting for connection on the server.
        while (m_server->connectionCount() == 0)
            std::this_thread::yield();
    }
};

TYPED_TEST_CASE_P(StunAsyncClientAcceptanceTest);

//-------------------------------------------------------------------------------------------------
// Test cases.

TYPED_TEST_P(StunAsyncClientAcceptanceTest, same_handler_cannot_be_added_twice)
{
    this->givenClientWithIndicationHandler();
    this->thenSameHandlerCannotBeAdded();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, add_remove_indication_handler)
{
    this->givenClientWithIndicationHandler();
    this->whenRemoveHandler();
    this->thenSameHandlerCanBeAddedAgain();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, reconnect_works)
{
    this->givenConnectedClient();
    this->whenRestartServer();
    this->thenClientReconnects();
    this->thenClientIsAbleToPerformRequests();
}

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    reconnect_handler_is_invoked_on_initial_connect)
{
    this->givenConnectedClient();
    this->andReconnectHandlerIsInvoked();
}

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    reconnect_occurs_after_initial_connect_failure)
{
    this->givenBrokenServer();
    this->givenClientFailedToConnect();

    this->whenRestartServer();

    this->thenClientReconnects();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, client_receives_indication)
{
    this->givenConnectedClient();
    this->whenServerSendsIndication();
    this->thenClientReceivesIndication();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, subscription_to_every_indication)
{
    this->subscribeToEveryIndication();

    this->givenConnectedClient();
    this->whenServerSendsIndication();
    this->thenClientReceivesIndication();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, client_receives_indication_after_reconnect)
{
    this->givenReconnectedClient();
    this->whenServerSendsIndication();
    this->thenClientReceivesIndication();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, request_scheduled_after_connection_forcibly_closed)
{
    this->givenConnectedClient();

    this->doInClientAioThread(
        [this]()
        {
            this->whenForciblyCloseClientConnection();
            this->whenIssueRequest();
        });

    this->thenClientReconnects();
    this->thenResponseIsReceived();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, scheduled_request_is_completed_after_reconnect)
{
    this->givenConnectedClient();

    this->doInClientAioThread(
        [this]()
        {
            this->whenIssueRequest();
            this->whenForciblyCloseClientConnection();
        });

    this->thenClientReconnects();
    this->thenRequestFailureIsReported();
}

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    request_result_is_reported_even_if_connect_always_fails)
{
    this->givenBrokenServer();
    this->givenDisconnectedClient();

    this->whenIssueRequest();

    this->thenRequestFailureIsReported();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, connection_closure_is_reported)
{
    this->givenConnectedClient();

    this->whenStopServer();
    this->thenClientReportsConnectionClosure();
}

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    connection_closure_is_not_reported_after_initial_connect_failure)
{
    this->givenBrokenServer();

    this->whenConnectToServer();

    this->thenConnectFailed();
    this->thenClientDoesNotReportConnectionClosure();
}

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    on_connection_closed_handler_triggered_more_than_once)
{
    this->givenReconnectedClient();
    this->whenStopServer();
    this->thenClientReportsConnectionClosure();
}

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    reconnecting_to_another_server_after_original_has_failed)
{
    this->givenConnectedClient();

    this->whenServerRestartedOnAnotherEndpoint();
    this->whenConnectToServer();

    this->thenClientConnected();
}

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    reconnecting_to_another_server_while_original_is_still_alive)
{
    this->givenConnectedClient();

    this->whenStartAnotherServer();
    this->doInClientAioThread(
        [this]()
        {
            this->whenDisconnectClient();
            this->whenConnectToNewServer();
        });

    this->thenClientConnected();
}

REGISTER_TYPED_TEST_CASE_P(StunAsyncClientAcceptanceTest,
    same_handler_cannot_be_added_twice,
    add_remove_indication_handler,
    reconnect_works,
    reconnect_handler_is_invoked_on_initial_connect,
    reconnect_occurs_after_initial_connect_failure,
    client_receives_indication,
    subscription_to_every_indication,
    client_receives_indication_after_reconnect,
    request_scheduled_after_connection_forcibly_closed,
    scheduled_request_is_completed_after_reconnect,
    request_result_is_reported_even_if_connect_always_fails,
    connection_closure_is_reported,
    connection_closure_is_not_reported_after_initial_connect_failure,
    on_connection_closed_handler_triggered_more_than_once,
    reconnecting_to_another_server_after_original_has_failed,
    reconnecting_to_another_server_while_original_is_still_alive);

} // namespace test
} // namespace stun
} // namespace nx
