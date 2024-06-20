// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include <nx/network/socket_factory.h>
#include <nx/network/stream_proxy.h>
#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace stun {
namespace test {

class AbstractStunServer
{
public:
    virtual ~AbstractStunServer() = default;

    virtual bool bind(const network::SocketAddress&) = 0;
    virtual bool listen() = 0;
    virtual nx::utils::Url url() const = 0;
    virtual void sendIndicationThroughEveryConnection(stun::Message) = 0;
    virtual nx::network::stun::MessageDispatcher& dispatcher() = 0;
    virtual std::size_t connectionCount() const = 0;
};

//-------------------------------------------------------------------------------------------------

class BasicStunAsyncClientAcceptanceTest:
    public ::testing::Test
{
public:
    ~BasicStunAsyncClientAcceptanceTest()
    {
        if (m_streamSocketFactoryBak)
            SocketFactory::setCreateStreamSocketFunc(std::move(*m_streamSocketFactoryBak));
    }

protected:
    void setSingleShotUnconnectableSocketFactory()
    {
        m_streamSocketFactoryBak = SocketFactory::setCreateStreamSocketFunc(
            [this](auto&&... args) {
                return createUnconnectableStreamSocket(std::forward<decltype(args)>(args)...); });
    }

private:
    class UnconnectableStreamSocket:
        public nx::network::TCPSocket
    {
    public:
        virtual void connectAsync(
            const SocketAddress& /*addr*/,
            nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
        {
            post([handler = std::move(handler)]() { handler(SystemError::connectionRefused); });
        }
    };

    std::optional<network::SocketFactory::CreateStreamSocketFuncType> m_streamSocketFactoryBak;
    nx::Mutex m_mutex;

    std::unique_ptr<network::AbstractStreamSocket> createUnconnectableStreamSocket(
        ssl::AdapterFunc /*verifyCertificateChainFunc*/,
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/,
        std::optional<int> /*ipVersion*/)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        SocketFactory::setCreateStreamSocketFunc(std::move(*m_streamSocketFactoryBak));
        m_streamSocketFactoryBak.reset();

        return std::make_unique<UnconnectableStreamSocket>();
    }
};

//-------------------------------------------------------------------------------------------------

class DroppingStream:
    public nx::utils::bstream::AbstractOutputConverter
{
public:
    DroppingStream(std::atomic<bool>* dropDataFlag):
        m_dropServerData(dropDataFlag)
    {
    }

    virtual int write(const void* data, size_t count) override
    {
        if (!*m_dropServerData)
            return m_outputStream->write(data, count);

        return count;
    }

private:
    std::atomic<bool>* m_dropServerData = nullptr;
};

//-------------------------------------------------------------------------------------------------

/**
 * @param AsyncClientTestTypes It is a struct with following nested types:
 * - ClientType Type that inherits nx::network::stun::AbstractAsyncClient.
 * - ServerType Class that implements AbstractStunServer interface.
 */
template<typename AsyncClientTestTypes>
class StunAsyncClientAcceptanceTest:
    public BasicStunAsyncClientAcceptanceTest
{
public:
    StunAsyncClientAcceptanceTest()
    {
        AbstractAsyncClient::Settings clientSettings;
        clientSettings.sendTimeout = kNoTimeout;
        clientSettings.recvTimeout = kNoTimeout;
        clientSettings.reconnectPolicy.initialDelay = std::chrono::milliseconds(1);
        m_client = std::make_unique<typename AsyncClientTestTypes::ClientType>(clientSettings);
    }

    ~StunAsyncClientAcceptanceTest()
    {
        m_client->pleaseStopSync();
    }

    void installMultipleCloseHandlers(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            auto& ctx = m_multipleCloseHandlers.emplace_back(
                std::make_unique<MultipleCloseHandlerContext>());
            m_client->addOnConnectionClosedHandler(
                [ctx = ctx.get()](SystemError::ErrorCode /**/){ ctx->closeEvent.push(); },
                ctx.get());
        }
    }

    void enableProxy()
    {
        m_proxy = std::make_unique<StreamProxy>();

        m_proxy->setDownStreamConverterFactory(
            [this]() { return std::make_unique<DroppingStream>(&m_dropServerData); });

        auto tcpServerSocket = std::make_unique<TCPServerSocket>(AF_INET);
        ASSERT_TRUE(tcpServerSocket->setNonBlockingMode(true));
        ASSERT_TRUE(tcpServerSocket->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(tcpServerSocket->listen());
        m_proxyAddress = tcpServerSocket->getLocalAddress();

        m_proxy->startProxy(
            std::make_unique<StreamServerSocketToAcceptorWrapper>(
                std::move(tcpServerSocket)),
            SocketAddress());
    }

protected:
    nx::Mutex m_serverSendResponseLock;

    void subscribeToEveryIndication()
    {
        m_indictionMethodToSubscribeTo = nx::network::stun::kEveryIndicationMethod;
    }

    template<typename Func>
    void doInClientAioThread(Func func)
    {
        nx::utils::promise<void> done;
        m_client->post(
            [&func, &done]()
            {
                func();
                done.set_value();
            });
        done.get_future().wait();
    }

    void givenClientWithIndicationHandler()
    {
        addIndicationHandler();
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

        thenClientReportedConnectionClosure();
        thenClientReconnects();
        // NOTE: Client reconnect does not mean server has initialized connection already.
        thenServerHasAtLeastOneConnection();
    }

    void thenServerHasAtLeastOneConnection()
    {
        while (m_server->connectionCount() == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

    void whenClientCancels()
    {
        m_client->cancelHandlersSync(this);
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
        sendRequest(m_testMethodNumber);
    }

    void whenForciblyCloseClientConnection()
    {
        m_client->closeConnection(SystemError::connectionReset);
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
        m_client->connect(
            serverUrl(),
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
        m_client->closeConnection(SystemError::connectionReset);
    }

    void whenConnectToNewServer()
    {
        whenConnectToServer();
    }

    void whenSilentlyDropAllConnectionsToServer()
    {
        ASSERT_TRUE(m_proxy);

        m_dropServerData = true;
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

    void thenSameHandlerCanBeAddedAgain()
    {
        addIndicationHandler();
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

    void thenResponseIsNotReceived()
    {
        ASSERT_FALSE(m_requestResult.pop(std::chrono::milliseconds(100)));
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

    void thenClientReportedConnectionClosure()
    {
        m_connectionClosedEventsReceived.pop();
    }

    void thenMultipleCloseHandlersWereInvoked(int n)
    {
        for (auto& ctx: m_multipleCloseHandlers)
        {
            for (int i = 0; i < n; ++i)
                ctx->closeEvent.pop();
        }
    }

    void thenClientReportedConnectionClosureWithResult(
        SystemError::ErrorCode expected)
    {
        ASSERT_EQ(expected, m_connectionClosedEventsReceived.pop());
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
        return *m_client;
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

    struct MultipleCloseHandlerContext
    {
        nx::utils::SyncQueue<void> closeEvent;
    };

    std::unique_ptr<typename AsyncClientTestTypes::ClientType> m_client;
    std::unique_ptr<typename AsyncClientTestTypes::ServerType> m_server;
    std::vector<std::unique_ptr<typename AsyncClientTestTypes::ServerType>> m_oldServers;
    SocketAddress m_serverEndpoint = SocketAddress::anyPrivateAddress;
    nx::utils::Url m_serverUrl;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectResults;
    nx::utils::SyncQueue<int /*dummy*/> m_reconnectEvents;
    nx::utils::SyncQueue<RequestResult> m_requestResult;
    nx::utils::SyncQueue<nx::network::stun::Message> m_indicationsReceived;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionClosedEventsReceived;
    RequestResult m_prevRequestResult;
    const int m_testMethodNumber = stun::MethodType::userMethod + 1;
    int m_indictionMethodToSubscribeTo = stun::MethodType::userMethod + 1;
    std::unique_ptr<StreamProxy> m_proxy;
    std::optional<SocketAddress> m_proxyAddress;
    std::atomic<bool> m_dropServerData{false};
    std::vector<std::unique_ptr<MultipleCloseHandlerContext>> m_multipleCloseHandlers;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        startServer();

        m_client->addOnReconnectedHandler(
            std::bind(&StunAsyncClientAcceptanceTest::onReconnected, this));
        m_client->addOnConnectionClosedHandler(
            std::bind(&StunAsyncClientAcceptanceTest::saveConnectionClosedEvent, this, _1), this);
    }

    void startServer()
    {
        using namespace std::placeholders;

        m_server = std::make_unique<typename AsyncClientTestTypes::ServerType>();

        m_server->dispatcher().registerRequestProcessor(
            m_testMethodNumber,
            [this](auto&&... args) { sendResponse(std::forward<decltype(args)>(args)...); });

        ASSERT_TRUE(m_server->bind(m_serverEndpoint));
        m_serverEndpoint = nx::network::url::getEndpoint(m_server->url());

        NX_VERBOSE(this, "Server started on %1", m_serverEndpoint);

        if (m_proxy)
            m_proxy->setProxyDestination(m_serverEndpoint);

        m_serverUrl = m_server->url();
        ASSERT_TRUE(m_server->listen());
    }

    void initializeClient()
    {
        using namespace std::placeholders;

        m_client->setIndicationHandler(
            m_indictionMethodToSubscribeTo,
            std::bind(&StunAsyncClientAcceptanceTest::saveIndication, this, _1),
            this);

        m_client->connect(
            serverUrl(),
            [this](SystemError::ErrorCode systemErrorCode)
            {
                m_connectResults.push(systemErrorCode);
            });
    }

    nx::utils::Url serverUrl() const
    {
        auto url = m_serverUrl;
        if (m_proxyAddress)
            url = network::url::Builder(url).setEndpoint(*m_proxyAddress);
        return url;
    }

    void sendRequest(int method)
    {
        using namespace std::placeholders;

        m_client->sendRequest(
            stun::Message(stun::Header(stun::MessageClass::request, method)),
            std::bind(&StunAsyncClientAcceptanceTest::storeRequestResult, this, _1, _2),
            this);
    }

    void sendResponse(nx::network::stun::MessageContext ctx)
    {
        NX_MUTEX_LOCKER lock(&m_serverSendResponseLock);

        nx::network::stun::Message response(stun::Header(
            stun::MessageClass::successResponse,
            ctx.message.header.method,
            ctx.message.header.transactionId));
        ctx.connection->sendMessage(std::move(response));
    }

    void storeRequestResult(
        SystemError::ErrorCode sysErrorCode,
        stun::Message response)
    {
        m_requestResult.push(RequestResult(sysErrorCode, std::move(response)));
    }

    void addIndicationHandler()
    {
        return m_client->setIndicationHandler(
            m_testMethodNumber + 1,
            [](nx::network::stun::Message) {},
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

    void saveIndication(nx::network::stun::Message indication)
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

TYPED_TEST_SUITE_P(StunAsyncClientAcceptanceTest);

//-------------------------------------------------------------------------------------------------
// Test cases.

TYPED_TEST_P(StunAsyncClientAcceptanceTest, add_remove_indication_handler)
{
    this->givenClientWithIndicationHandler();
    this->whenClientCancels();
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
    this->thenClientReportedConnectionClosure();
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
    this->thenClientReportedConnectionClosure();
}

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    multiple_connection_closed_handlers_multiple_times)
{
    this->installMultipleCloseHandlers(10);

    this->givenReconnectedClient();
    this->whenStopServer();
    this->thenClientReportedConnectionClosure();

    this->thenMultipleCloseHandlersWereInvoked(2);
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

TYPED_TEST_P(
    StunAsyncClientAcceptanceTest,
    response_is_not_delivered_after_cancel)
{
    this->givenConnectedClient();

    this->doInClientAioThread(
        [this]()
        {
            NX_MUTEX_LOCKER lock(&this->m_serverSendResponseLock);

            this->whenIssueRequest();
            this->whenClientCancels();
        });

    this->thenResponseIsNotReceived();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(StunAsyncClientAcceptanceTest);

REGISTER_TYPED_TEST_SUITE_P(StunAsyncClientAcceptanceTest,
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
    multiple_connection_closed_handlers_multiple_times,
    reconnecting_to_another_server_after_original_has_failed,
    reconnecting_to_another_server_while_original_is_still_alive,
    response_is_not_delivered_after_cancel);

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
