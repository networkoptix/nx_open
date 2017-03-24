#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/sync_queue.h>

#include <common/common_globals.h>
#include <utils/common/guard.h>
#include <utils/common/long_runnable.h>
#include <utils/media/custom_output_stream.h>

#include "repeating_buffer_sender.h"

namespace nx_http {
namespace test {

class SslAssertHandler:
    public nx_http::AbstractHttpRequestHandler
{
public:
    SslAssertHandler(bool expectSsl):
        m_expectSsl(expectSsl)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        EXPECT_EQ(m_expectSsl, connection->isSsl());
        completionHandler(nx_http::RequestResult(
            nx_http::StatusCode::ok,
            std::make_unique<nx_http::BufferSource>(
                nx_http::BufferType("text/plain"), nx_http::BufferType("ok"))));
    }

private:
    const bool m_expectSsl;
};

class AsyncHttpClient:
    public ::testing::Test
{
public:
    AsyncHttpClient():
        m_testHttpServer(std::make_unique<TestHttpServer>())
    {
        m_testHttpServer->registerRequestProcessor<SslAssertHandler>(
            lit("/httpOnly"), []() { return std::make_unique<SslAssertHandler>(false); });

        m_testHttpServer->registerRequestProcessor<SslAssertHandler>(
            lit("/httpsOnly"), []() { return std::make_unique<SslAssertHandler>(true); });
    }

    TestHttpServer* testHttpServer()
    {
        return m_testHttpServer.get();
    }

protected:
    std::unique_ptr<TestHttpServer> m_testHttpServer;

    void httpsTest(const QString& scheme, const QString& path)
    {
        const QUrl url(lit("%1://%2/%3")
            .arg(scheme).arg(m_testHttpServer->serverAddress().toString()).arg(path));

        nx::utils::promise<void> promise;
        NX_LOGX(lm("httpsTest: %1").str(url), cl_logINFO);

        const auto client = nx_http::AsyncHttpClient::create();
        client->doGet(url,
            [&promise](AsyncHttpClientPtr ptr)
            {
                 EXPECT_TRUE(ptr->hasRequestSuccesed());
                 promise.set_value();
            });

        promise.get_future().wait();
    }

    void testResult(const QString& path, const nx_http::BufferType& expectedResult)
    {
        const QUrl url(lit("http://%1%2")
            .arg(m_testHttpServer->serverAddress().toString()).arg(path));

        nx::utils::promise<void> promise;
        NX_LOGX(lm("testResult: %1").str(url), cl_logINFO);

        const auto client = nx_http::AsyncHttpClient::create();
        client->doGet(url,
            [&promise, &expectedResult](AsyncHttpClientPtr ptr)
            {
                 EXPECT_TRUE(ptr->hasRequestSuccesed());
                 EXPECT_EQ(expectedResult, ptr->fetchMessageBodyBuffer());
                 promise.set_value();
            });

        promise.get_future().wait();
    }
};

TEST_F(AsyncHttpClient, Https)
{
    ASSERT_TRUE(m_testHttpServer->bindAndListen());
    httpsTest(lit("http"), lit("httpOnly"));
    httpsTest(lit("https"), lit("httpsOnly"));
}

// TODO: #mux Better create HttpServer test and move it there.
TEST_F(AsyncHttpClient, ServerModRewrite)
{
    m_testHttpServer->registerStaticProcessor(
        nx_http::kAnyPath, "root", "text/plain");
    m_testHttpServer->registerStaticProcessor(
        lit("/somePath1/longerPath"), "someData1", "text/plain");
    m_testHttpServer->registerStaticProcessor(
        lit("/somePath2/longerPath"), "someData2", "text/plain");
    ASSERT_TRUE(m_testHttpServer->bindAndListen());

    testResult(lit("/"), "root");
    testResult(lit("/somePath1/longerPath"), "someData1");
    testResult(lit("/somePath2/longerPath"), "someData2");
    testResult(lit("/somePath3/longerPath"), "root");
    testResult(lit("/suffix/somePath1/longerPath"), "root");

    m_testHttpServer->addModRewriteRule(lit("/somePath2/longerPath"), lit("/"));
    m_testHttpServer->addModRewriteRule(lit("/somePath3/"), lit("/somePath1/"));

    testResult(lit("/"), "root");
    testResult(lit("/somePath1/longerPath"), "someData1");
    testResult(lit("/somePath2/longerPath"), "root");
    testResult(lit("/somePath3/longerPath"), "someData1");
    testResult(lit("/suffix/somePath1/longerPath"), "root");

    m_testHttpServer->addModRewriteRule(lit("/suffix/"), lit("/"));

    testResult(lit("/"), "root");
    testResult(lit("/somePath1/longerPath"), "someData1");
    testResult(lit("/somePath2/longerPath"), "root");
    testResult(lit("/somePath3/longerPath"), "someData1");
    testResult(lit("/suffix/somePath1/longerPath"), "someData1");
}

//TODO #ak introduce built-in http server to automate AsyncHttpClient tests

namespace {
static void testHttpClientForFastRemove(const QUrl& url)
{
    // use different delays (10us - 0.5s) to catch problems on different stages
    for (uint time = 10; time < 500000; time *= 2)
    {
        const auto client = nx_http::AsyncHttpClient::create();
        client->doGet(url);

        // kill the client after some delay
        std::this_thread::sleep_for(std::chrono::microseconds(time));
        client->pleaseStopSync();
    }
}
} // namespace

TEST_F(AsyncHttpClient, FastRemove)
{
    testHttpClientForFastRemove(lit("http://127.0.0.1/"));
    testHttpClientForFastRemove(lit("http://localhost/"));
    testHttpClientForFastRemove(lit("http://doestNotExist.host/"));

    testHttpClientForFastRemove(lit("https://127.0.0.1/"));
    testHttpClientForFastRemove(lit("https://localhost/"));
    testHttpClientForFastRemove(lit("https://doestNotExist.host/"));
}

TEST_F(AsyncHttpClient, FastRemoveBadHost)
{
    QUrl url(lit("http://doestNotExist.host/"));

    for (int i = 0; i < 1000; ++i)
    {
        const auto client = nx_http::AsyncHttpClient::create();
        client->doGet(url);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

TEST_F(AsyncHttpClient, motionJpegRetrieval)
{
    static const int CONCURRENT_CLIENT_COUNT = 100;

    const nx::Buffer frame1 =
        "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxx\r\nxxxxxxxxxxxxxxxx2";
    const nx::String boundary = "fbdr";

    const nx::Buffer testFrame =
        "\r\n--" + boundary +
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        + frame1;

    m_testHttpServer->registerRequestProcessor<RepeatingBufferSender>(
        "/mjpg",
        [&]()->std::unique_ptr<RepeatingBufferSender>
        {
            return std::make_unique<RepeatingBufferSender>(
                "multipart/x-mixed-replace;boundary=" + boundary,
                testFrame);
        });

    ASSERT_TRUE(m_testHttpServer->bindAndListen());

    //fetching mjpeg with async http client
    const QUrl url(lit("http://127.0.0.1:%1/mjpg").arg(m_testHttpServer->serverAddress().port));

    struct ClientContext
    {
        ~ClientContext()
        {
            client->pleaseStopSync();
            client.reset(); //ensuring client removed before multipartParser
        }

        nx_http::AsyncHttpClientPtr client;
        nx_http::MultipartContentParser multipartParser;
    };

    std::atomic<size_t> bytesProcessed(0);

    auto checkReceivedContentFunc = [&](const QnByteArrayConstRef& data) {
        ASSERT_EQ(data, frame1);
        bytesProcessed += data.size();
    };

    std::vector<ClientContext> clients(CONCURRENT_CLIENT_COUNT);
    for (ClientContext& clientCtx : clients)
    {
        clientCtx.client = nx_http::AsyncHttpClient::create();
        clientCtx.multipartParser.setNextFilter(makeCustomOutputStream(checkReceivedContentFunc));
        QObject::connect(
            clientCtx.client.get(), &nx_http::AsyncHttpClient::responseReceived,
            clientCtx.client.get(),
            [&](nx_http::AsyncHttpClientPtr client)
            {
                ASSERT_TRUE(client->response() != nullptr);
                ASSERT_EQ(client->response()->statusLine.statusCode, nx_http::StatusCode::ok);
                auto contentTypeIter = client->response()->headers.find("Content-Type");
                ASSERT_TRUE(contentTypeIter != client->response()->headers.end());
                clientCtx.multipartParser.setContentType(contentTypeIter->second);
            },
            Qt::DirectConnection);
        QObject::connect(
            clientCtx.client.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
            clientCtx.client.get(),
            [&](nx_http::AsyncHttpClientPtr client)
            {
                clientCtx.multipartParser.processData(client->fetchMessageBodyBuffer());
            },
            Qt::DirectConnection);
        clientCtx.client->doGet(url);
    }

    std::this_thread::sleep_for(std::chrono::seconds(7));

    clients.clear();
}

class AsyncHttpClientTestMultiRequest:
    public AsyncHttpClient
{
public:
    AsyncHttpClientTestMultiRequest():
        m_client(nx_http::AsyncHttpClient::create()),
        m_requestFinished(false)
    {
        init();
    }

protected:
    void doRequest(const QUrl& url, const QByteArray& message)
    {
        static const int kWaitTimeoutMs = 1000 * 10;

        QnMutexLocker lock(&m_mutex);
        m_requestFinished = false;
        m_expectedResponse = message;
        m_client->doGet(url);
        while (!m_requestFinished)
            ASSERT_TRUE(m_waitCond.wait(&m_mutex, kWaitTimeoutMs));
    }

    void doMultipleRequestsReusingHttpClient()
    {
        for (const auto& ctx: m_requests)
            doRequest(ctx.url, ctx.message);
    }

private:
    struct TestRequestContext
    {
        QUrl url;
        QByteArray message;
    };

    std::vector<TestRequestContext> m_requests;

    nx_http::AsyncHttpClientPtr m_client;
    QnMutex m_mutex;
    QnWaitCondition m_waitCond;
    bool m_requestFinished;
    QByteArray m_expectedResponse;

    void init()
    {
        const std::size_t testRequestCount = 3;
        m_requests.resize(testRequestCount);

        for (std::size_t i = 0; i < m_requests.size(); ++i)
        {
            m_requests[i].url = QUrl(
                lit("http://127.0.0.1/AsyncHttpClientTestMultiRequest_%1").arg(i));
            m_requests[i].message = lit("SimpleMessage_%1").arg(i).toLatin1();
        }

        for (const auto& ctx: m_requests)
        {
            ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
                ctx.url.path(),
                ctx.message,
                "application/text"));
        }

        ASSERT_TRUE(testHttpServer()->bindAndListen());

        for (auto& ctx: m_requests)
            ctx.url.setPort(testHttpServer()->serverAddress().port);

        QObject::connect(
            m_client.get(), &nx_http::AsyncHttpClient::done,
            m_client.get(),
            [this](nx_http::AsyncHttpClientPtr client) { onDone(std::move(client)); },
            Qt::DirectConnection);
    }

    void onDone(nx_http::AsyncHttpClientPtr client)
    {
        ASSERT_FALSE(client->failed()) << "Response: " <<
            (client->response() == nullptr
                ? std::string("null")
                : client->response()->statusLine.reasonPhrase.toStdString());
        client->socket()->cancelIOSync(nx::network::aio::etNone);

        ASSERT_EQ(nx_http::StatusCode::ok, client->response()->statusLine.statusCode);
        ASSERT_EQ(m_expectedResponse, client->fetchMessageBodyBuffer());
        auto contentTypeIter = client->response()->headers.find("Content-Type");
        ASSERT_TRUE(contentTypeIter != client->response()->headers.end());

        QnMutexLocker lock(&m_mutex);
        m_requestFinished = true;
        m_waitCond.wakeAll();
    }
};

TEST_F(AsyncHttpClientTestMultiRequest, server_does_not_support_persistent_connections)
{
    testHttpServer()->setPersistentConnectionEnabled(false);
    doMultipleRequestsReusingHttpClient();
}

TEST_F(AsyncHttpClientTestMultiRequest, server_supports_persistent_connections)
{
    testHttpServer()->setPersistentConnectionEnabled(true);
    doMultipleRequestsReusingHttpClient();
}

TEST_F(AsyncHttpClient, ConnectionBreak)
{
    static const QByteArray kResponse(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 100\r\n\r\n"
        "not enough content");

    nx::utils::promise<int> serverPort;
    nx::utils::thread serverThread(
        [&]()
        {
            const auto server = std::make_unique<nx::network::TCPServerSocket>(
                SocketFactory::tcpClientIpVersion());

            ASSERT_TRUE(server->bind(SocketAddress::anyAddress));
            ASSERT_TRUE(server->listen());
            serverPort.set_value(server->getLocalAddress().port);

            std::unique_ptr<AbstractStreamSocket> client(server->accept());
            ASSERT_TRUE((bool)client);

            QByteArray buffer(1024, Qt::Uninitialized);
            int size = 0;
            while (!buffer.left(size).contains("\r\n\r\n"))
            {
                auto recv = client->recv(buffer.data() + size, buffer.size() - size);
                ASSERT_GT(recv, 0);
                size += recv;
            }

            buffer.resize(size);
            ASSERT_EQ(kResponse.size(), client->send(kResponse.data(), kResponse.size()));
    });

    auto client = nx_http::AsyncHttpClient::create();
    nx::utils::promise<void> clientDone;
    QObject::connect(
        client.get(), &nx_http::AsyncHttpClient::done,
        [&](nx_http::AsyncHttpClientPtr client)
        {
            EXPECT_TRUE(client->failed());
            EXPECT_EQ(QByteArray("not enough content"), client->fetchMessageBodyBuffer());
            EXPECT_EQ(SystemError::connectionReset, client->lastSysErrorCode());
            clientDone.set_value();
        });

    client->doGet(lit("http://127.0.0.1:%1/test").arg(serverPort.get_future().get()));
    serverThread.join();
    clientDone.get_future().wait();
}

namespace {
class TestHandler:
    public nx_http::AbstractHttpRequestHandler
{
public:
    TestHandler(int requestNumber):
        m_mimeType("text/plain"),
        m_response("qweqweqwewqeqwe"),
        m_requestNumber(requestNumber)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        if (m_requestNumber > 0)
            connection->takeSocket(); //< Closing connection by destroying socket.

        completionHandler(
            nx_http::RequestResult(
                nx_http::StatusCode::ok,
                std::make_unique< nx_http::BufferSource >(m_mimeType, m_response)));
    }

private:
    const nx_http::StringType m_mimeType;
    const QByteArray m_response;
    int m_requestNumber;
};
} // namespace

TEST_F(AsyncHttpClient, ConnectionBreakAfterReceivingSecondRequest)
{
    static const char* testPath = "/ConnectionBreakAfterReceivingSecondRequest";

    std::atomic<int> requestCounter(0);
    ASSERT_TRUE(
        testHttpServer()->registerRequestProcessor<TestHandler>(
            testPath,
            [&requestCounter]() mutable
            {
                return std::make_unique<TestHandler>(requestCounter++);
            }));
    ASSERT_TRUE(testHttpServer()->bindAndListen());

    QUrl testUrl(lit("http://%1%2")
        .arg(testHttpServer()->serverAddress().toString())
        .arg(QString::fromLatin1(testPath)));

    nx_http::HttpClient httpClient;
    ASSERT_TRUE(httpClient.doGet(testUrl));
    ASSERT_NE(nullptr, httpClient.response());
    ASSERT_EQ(SystemError::noError, httpClient.lastSysErrorCode());
    ASSERT_FALSE(httpClient.doGet(testUrl));
    ASSERT_NE(SystemError::noError, httpClient.lastSysErrorCode());
}

//-------------------------------------------------------------------------------------------------

class AsyncHttpClientReliability:
    public ::testing::Test
{
public:
    constexpr static int kNumberOfClients = 100;

    AsyncHttpClientReliability():
        m_testServer(false, nx::network::NatTraversalSupport::disabled)
    {
    }

    virtual ~AsyncHttpClientReliability() override
    {
        for (auto& clientContext: m_clients)
            clientContext.client->pleaseStopSync();

        m_testServer.pleaseStopSync();
    }

protected:
    void givenRandomlyFailingServer()
    {
        static const char httpResponse[] = R"http(
            HTTP/1.1 200 OK
            Date: Wed, 19 Oct 2016 13:24:10 +0000
            Server: Nx Witness/3.0.0.13295 (Network Optix) Apache/2.4.16 (Unix)
            Content-Type: text/plain
            Content-Length: 12

            Hello, world
        )http";

        m_testServer.setResponseBuffer(httpResponse);

        ASSERT_TRUE(m_testServer.bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_TRUE(m_testServer.listen());
        m_serverEndpoint = m_testServer.address();
    }

    void givenMultipleHttpClients()
    {
        m_clients.resize(kNumberOfClients);
        for (auto& clientContext: m_clients)
        {
            clientContext.client = nx_http::AsyncHttpClient::create();
            clientContext.client->setResponseReadTimeoutMs(500);
            clientContext.client->setMessageBodyReadTimeoutMs(500);
            clientContext.requestsToSend = nx::utils::random::number<int>(1, 3);
        }
    }

    void whenEachClientHasPerformedRequestToTheServer()
    {
        issueRequests();
        waitForEachRequestCompletion();
    }

    void thenEachClientShouldHaveCorrectState()
    {
        for (auto& clientContext: m_clients)
        {
            if (!clientContext.client->failed())
            {
                ASSERT_NE(nullptr, clientContext.client->response());
            }
            else
            {
                ASSERT_EQ(nullptr, clientContext.client->response());
            }
        }
    }

private:
    struct ClientContext
    {
        nx_http::AsyncHttpClientPtr client;
        nx::utils::promise<void> requestDonePromise;
        int requestsToSend;
    };

    RandomlyFailingHttpServer m_testServer;
    SocketAddress m_serverEndpoint;
    std::vector<ClientContext> m_clients;

    QUrl serverUrl() const
    {
        return QUrl(lit("http://%1/tst").arg(m_serverEndpoint.toString()));
    }

    void issueRequests()
    {
        for (auto& clientContext: m_clients)
            issueRequest(&clientContext);
    }

    void issueRequest(ClientContext* clientContext)
    {
        using namespace std::placeholders;

        clientContext->client->doGet(
            serverUrl(),
            std::bind(&AsyncHttpClientReliability::onRequestDone, this, _1, clientContext));
    }

    void waitForEachRequestCompletion()
    {
        for (auto& clientContext: m_clients)
            clientContext.requestDonePromise.get_future().wait();
    }

    void onRequestDone(nx_http::AsyncHttpClientPtr /*client*/, ClientContext* clientContext)
    {
        --clientContext->requestsToSend;
        if (clientContext->requestsToSend == 0)
        {
            clientContext->requestDonePromise.set_value();
            return;
        }
        issueRequest(clientContext);
    }
};

TEST_F(AsyncHttpClientReliability, BreakingConnectionAtRandomPoint)
{
    givenRandomlyFailingServer();
    givenMultipleHttpClients();
    whenEachClientHasPerformedRequestToTheServer();
    thenEachClientShouldHaveCorrectState();
}

//-------------------------------------------------------------------------------------------------
// AsyncHttpClientReusingConnection

class AsyncHttpClientReusingConnection:
    public AsyncHttpClient
{
public:
    static const char* testPath;

    AsyncHttpClientReusingConnection():
        m_responseCount(0)
    {
        m_httpClient = nx_http::AsyncHttpClient::create();
        m_httpClient->setSendTimeoutMs(1000);
        QObject::connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::done,
            [this](AsyncHttpClientPtr client)
            {
                onRequestCompleted(std::move(client));
            });
    }

    ~AsyncHttpClientReusingConnection()
    {
        m_httpClient->pleaseStopSync();
        m_testHttpServer.reset();
    }

protected:
    void initializeStaticContentServer()
    {
        testHttpServer()->setPersistentConnectionEnabled(false);
        NX_GTEST_ASSERT_TRUE(
            testHttpServer()->registerStaticProcessor(
                testPath,
                "qwade324dwfasd123sdf23sdfsdf",
                "text/plain"));
        NX_GTEST_ASSERT_TRUE(testHttpServer()->bindAndListen());

        m_testUrl = lm("http://%1%2").str(testHttpServer()->serverAddress()).arg(testPath);
    }

    void initializeDelayedConnectionClosureServer()
    {
        using namespace std::placeholders;

        testHttpServer()->setPersistentConnectionEnabled(true);

        auto httpHandlerFunc =
            std::bind(&AsyncHttpClientReusingConnection::delayedConnectionClosureHttpHandlerFunc,
                this, _1, _2, _3, _4, _5);
        NX_GTEST_ASSERT_TRUE(
            testHttpServer()->registerRequestProcessorFunc(
                testPath, std::move(httpHandlerFunc)));
        NX_GTEST_ASSERT_TRUE(testHttpServer()->bindAndListen());

        m_testUrl = lm("http://%1%2").str(testHttpServer()->serverAddress()).arg(testPath);
    }

    void scheduleRequestToValidUrlJustAfterFirstRequest()
    {
        m_scheduledRequests.push(m_testUrl);
    }

    void performRequestToValidUrl()
    {
        scheduleRequestToValidUrlJustAfterFirstRequest();
        sendNextRequest();
    }

    void performRequestToInvalidUrl()
    {
        m_scheduledRequests.push(QUrl("http://example.com:58249/test"));
        sendNextRequest();
    }

    void assertRequestSucceeded()
    {
        ASSERT_NE(nullptr, m_responseQueue.pop());
    }

    void assertRequestFailed()
    {
        ASSERT_EQ(nullptr, m_responseQueue.pop());
    }

private:
    struct HttpConnectionContext
    {
        nx::network::aio::Timer timer;
        int requestsReceived = 0;
    };

    QUrl m_testUrl;
    nx::utils::SyncQueue<std::unique_ptr<nx_http::Response>> m_responseQueue;
    std::atomic<int> m_responseCount;
    nx_http::AsyncHttpClientPtr m_httpClient;
    std::queue<QUrl> m_scheduledRequests;
    std::map<
        nx_http::HttpServerConnection*,
        std::unique_ptr<HttpConnectionContext>> m_connectionToContext;
    QnMutex m_mutex;

    void sendNextRequest()
    {
        const auto scheduledRequestUrl = m_scheduledRequests.front();
        m_scheduledRequests.pop();
        m_httpClient->doGet(scheduledRequestUrl);
    }

    void onRequestCompleted(AsyncHttpClientPtr client)
    {
        ++m_responseCount;
        if (client->response() && !client->failed())
            m_responseQueue.push(std::make_unique<nx_http::Response>(*client->response()));
        else
            m_responseQueue.push(nullptr);
        if (m_responseCount >= 2)
            return;

        if (m_scheduledRequests.empty())
            return;

        sendNextRequest();
    }

    void delayedConnectionClosureHttpHandlerFunc(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        nx_http::RequestResult requestResult(nx_http::StatusCode::ok);

        HttpConnectionContext* connectionContext = nullptr;
        {
            QnMutexLocker lock(&m_mutex);
            auto p = m_connectionToContext.emplace(connection, nullptr);
            if (p.second)
                p.first->second = std::make_unique<HttpConnectionContext>();
            connectionContext = p.first->second.get();
        }

        ++connectionContext->requestsReceived;

        requestResult.connectionEvents.onResponseHasBeenSent =
            std::bind(&AsyncHttpClientReusingConnection::onResponseSent, this, connection);
        connection->registerCloseHandler(
            std::bind(&AsyncHttpClientReusingConnection::onConnectionClosed, this, connection));

        completionHandler(std::move(requestResult));
    }

    void onResponseSent(nx_http::HttpServerConnection* connection)
    {
        QnMutexLocker lock(&m_mutex);

        auto it = m_connectionToContext.find(connection);
        if (it == m_connectionToContext.end())
            return;

        it->second->timer.bindToAioThread(connection->getAioThread());
        it->second->timer.start(
            std::chrono::milliseconds(nx::utils::random::number<int>(1, 30)),
            std::bind(&AsyncHttpClientReusingConnection::closeConnection, this, connection));
    }

    void closeConnection(nx_http::HttpServerConnection* connection)
    {
        connection->socket()->shutdown();
        connection->closeConnection(SystemError::connectionReset);
    }

    void onConnectionClosed(nx_http::HttpServerConnection* connection)
    {
        QnMutexLocker lock(&m_mutex);

        auto it = m_connectionToContext.find(connection);
        if (it != m_connectionToContext.end())
            m_connectionToContext.erase(it);
    }
};

const char* AsyncHttpClientReusingConnection::testPath = "/ReusingExistingConnection";

TEST_F(AsyncHttpClientReusingConnection, connecting_to_the_valid_url_first)
{
    initializeStaticContentServer();
    scheduleRequestToValidUrlJustAfterFirstRequest();
    performRequestToValidUrl();
    assertRequestSucceeded();
    assertRequestSucceeded();
}

TEST_F(AsyncHttpClientReusingConnection, connecting_to_the_invalid_url_first)
{
    initializeStaticContentServer();
    scheduleRequestToValidUrlJustAfterFirstRequest();
    performRequestToInvalidUrl();
    assertRequestFailed();
    assertRequestSucceeded();
}

TEST_F(AsyncHttpClientReusingConnection, server_closes_connection_with_random_delay)
{
    initializeDelayedConnectionClosureServer();
    scheduleRequestToValidUrlJustAfterFirstRequest();
    performRequestToValidUrl();
    assertRequestSucceeded();
    assertRequestSucceeded();
}

//-------------------------------------------------------------------------------------------------

class TestTcpServer:
    public QnLongRunnable
{
public:
    TestTcpServer(std::vector<QByteArray> dataToSend):
        m_delayAfterSend(200),
        m_dataToSend(std::move(dataToSend)),
        m_serverSocket(SocketFactory::tcpClientIpVersion()),
        m_terminated(false)
    {
    }

    ~TestTcpServer()
    {
        m_terminated = true;
        stop();
    }

    bool bindAndListen()
    {
        if (!m_serverSocket.bind(SocketAddress(HostAddress::localhost, 0)) ||
            !m_serverSocket.listen())
        {
            return false;
        }
        return true;
    }

    SocketAddress endpoint() const
    {
        return m_serverSocket.getLocalAddress();
    }

protected:
    virtual void run() override
    {
        constexpr const std::chrono::milliseconds recvTimeout =
            std::chrono::milliseconds(500);

        m_serverSocket.setRecvTimeout(recvTimeout.count());
        while (!m_terminated)
        {
            std::unique_ptr<AbstractStreamSocket> connection(m_serverSocket.accept());
            if (!connection)
            {
                std::this_thread::sleep_for(recvTimeout);
                continue;

            }
            for (const auto& buf: m_dataToSend)
            {
                ASSERT_EQ(buf.size(), connection->send(buf));
                std::this_thread::sleep_for(m_delayAfterSend);
            }
        }
    }

private:
    const std::chrono::milliseconds m_delayAfterSend;
    std::vector<QByteArray> m_dataToSend;
    nx::network::TCPServerSocket m_serverSocket;
    bool m_terminated;
};

TEST_F(AsyncHttpClient, PartionedIncomingData)
{
    std::vector<QByteArray> dataToSend;

    dataToSend.push_back(
    R"http(
HTTP/1.1 401 Unauthorized
Date: Wed, 19 Oct 2016 13:24:10 +0000
Server: Nx Witness/3.0.0.13295 (Network Optix) Apache/2.4.16 (Unix)
Content-Type: application/json
Content-Length: 103
WWW-Authenticate: Digest algorithm="MD5", nonce="15185146660140348415", realm="VMS"
X-Nx-Result-Code: notAuthorized

)http");

    dataToSend.push_back(
    R"http(
{"errorClass":"unauthorized","errorDetail":100,"errorText":"unauthorized","resultCode":"notAuthorized"}HTTP/1.1 200 OK
Date: Wed, 19 Oct 2016 13:24:10 +0000
Server: Nx Witness/3.0.0.13295 (Network Optix) Apache/2.4.16 (Unix)
Content-Type: application/json
Content-Length: 103
X-Nx-Result-Code: ok

)http");

    dataToSend.push_back(
    R"http(
{"errorClass":"unauthorized","errorDetail":100,"errorText":"unauthorized","resultCode":"notAuthorized"}
    )http");

    TestTcpServer testTcpServer(std::move(dataToSend));
    ASSERT_TRUE(testTcpServer.bindAndListen())
        << SystemError::getLastOSErrorText().toStdString();
    testTcpServer.start();

    QUrl url(lit("http://%1/secret/path").arg(testTcpServer.endpoint().toString()));
    url.setUserName("don't tell");
    url.setPassword("anyone");
    nx_http::HttpClient httpClient;
    ASSERT_TRUE(httpClient.doGet(url));
    ASSERT_NE(nullptr, httpClient.response());
    ASSERT_EQ(nx_http::StatusCode::ok, httpClient.response()->statusLine.statusCode);
}

} // namespace test
} // namespace nx_http
