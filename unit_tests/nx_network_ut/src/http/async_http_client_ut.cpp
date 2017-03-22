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

class AsyncHttpClientTest:
    public ::testing::Test
{
public:
    AsyncHttpClientTest():
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

TEST_F(AsyncHttpClientTest, Https)
{
    ASSERT_TRUE(m_testHttpServer->bindAndListen());
    httpsTest(lit("http"), lit("httpOnly"));
    httpsTest(lit("https"), lit("httpsOnly"));
}

// TODO: #mux Better create HttpServer test and move it there.
TEST_F(AsyncHttpClientTest, ServerModRewrite)
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

TEST_F(AsyncHttpClientTest, FastRemove)
{
    testHttpClientForFastRemove(lit("http://127.0.0.1/"));
    testHttpClientForFastRemove(lit("http://localhost/"));
    testHttpClientForFastRemove(lit("http://doestNotExist.host/"));

    testHttpClientForFastRemove(lit("https://127.0.0.1/"));
    testHttpClientForFastRemove(lit("https://localhost/"));
    testHttpClientForFastRemove(lit("https://doestNotExist.host/"));
}

TEST_F(AsyncHttpClientTest, FastRemoveBadHost)
{
    QUrl url(lit("http://doestNotExist.host/"));

    for (int i = 0; i < 1000; ++i)
    {
        const auto client = nx_http::AsyncHttpClient::create();
        client->doGet(url);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

TEST_F(AsyncHttpClientTest, motionJpegRetrieval)
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

TEST_F(AsyncHttpClientTest, MultiRequestTest)
{
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test",
        "SimpleTest",
        "application/text"));
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test2",
        "SimpleTest2",
        "application/text"));
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test3",
        "SimpleTest3",
        "application/text"));
    ASSERT_TRUE(testHttpServer()->bindAndListen());

    const QUrl url(lit("http://127.0.0.1:%1/test")
        .arg(testHttpServer()->serverAddress().port));
    const QUrl url2(lit("http://127.0.0.1:%1/test2")
        .arg(testHttpServer()->serverAddress().port));
    const QUrl url3(lit("http://127.0.0.1:%1/test3")
        .arg(testHttpServer()->serverAddress().port));

    auto client = nx_http::AsyncHttpClient::create();

    QByteArray expectedResponse;

    QnMutex mutex;
    QnWaitCondition waitCond;
    bool requestFinished = false;

    QObject::connect(
        client.get(), &nx_http::AsyncHttpClient::done,
        client.get(),
        [&](nx_http::AsyncHttpClientPtr client)
        {
            ASSERT_FALSE(client->failed()) << "Response: " << 
                (client->response() == nullptr
                    ? std::string("null")
                    : client->response()->statusLine.reasonPhrase.toStdString());
            ASSERT_EQ(nx_http::StatusCode::ok, client->response()->statusLine.statusCode);
            ASSERT_EQ(expectedResponse, client->fetchMessageBodyBuffer());
            auto contentTypeIter = client->response()->headers.find("Content-Type");
            ASSERT_TRUE(contentTypeIter != client->response()->headers.end());

            QnMutexLocker lock(&mutex);
            requestFinished = true;
            waitCond.wakeAll();
        },
        Qt::DirectConnection);

    static const int kWaitTimeoutMs = 1000 * 10;
    // step 0: check if client reconnect smoothly if server already dropped connection
    // step 1: check 2 requests in a row via same connection
    for (int i = 0; i < 2; ++i)
    {
        testHttpServer()->setPersistentConnectionEnabled(i != 0);

        {
            QnMutexLocker lock(&mutex);
            requestFinished = false;
            expectedResponse = "SimpleTest";
            client->doGet(url);
            while (!requestFinished)
                ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));
        }

        {
            QnMutexLocker lock(&mutex);
            requestFinished = false;
            expectedResponse = "SimpleTest2";
            client->doGet(url2);
            while (!requestFinished)
                ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));
        }

        {
            QnMutexLocker lock(&mutex);
            requestFinished = false;
            expectedResponse = "SimpleTest3";
            client->doGet(url3);
            while (!requestFinished)
                ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));
        }

    }
}

TEST_F(AsyncHttpClientTest, ConnectionBreak)
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

TEST_F(AsyncHttpClientTest, ConnectionBreakAfterReceivingSecondRequest)
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
// AsyncHttpClientCorrectUrlTransferring

class AsyncHttpClientCorrectUrlTransferring:
    public AsyncHttpClientTest
{
public:
    AsyncHttpClientCorrectUrlTransferring()
    {
        init();
    }

protected:
    QString testPath() const
    {
        return "/validateUrl";
    }

    void whenIssuedRequestWithEncodedSequenceInQueryAndFragment()
    {
        const auto query = QUrl::toPercentEncoding("test#%20#");
        const auto fragment = QUrl::toPercentEncoding("#frag%20ment");

        m_testUrl = QUrl(lit("http://%1%2?%3#%4")
            .arg(testHttpServer()->serverAddress().toString())
            .arg(testPath()).arg(QLatin1String(query)).arg(QLatin1String(fragment)));

        ASSERT_TRUE(m_httpClient.doGet(m_testUrl));
    }

    void assertServerHasReceivedCorrectUrl()
    {
        auto url = m_urlsFromReceivedRequests.pop();
        url.setHost(m_testUrl.host());
        url.setPort(m_testUrl.port());
        url.setScheme(m_testUrl.scheme());
        ASSERT_EQ(m_testUrl, url);
    }

private:
    nx::utils::SyncQueue<QUrl> m_urlsFromReceivedRequests;
    nx_http::HttpClient m_httpClient;
    QUrl m_testUrl;

    void init()
    {
        using namespace std::placeholders;

        ASSERT_TRUE(
            testHttpServer()->registerRequestProcessor(
                testPath(),
                std::bind(&AsyncHttpClientCorrectUrlTransferring::onRequestReceived, this,
                    _1, _2, _3, _4, _5)));

        ASSERT_TRUE(testHttpServer()->bindAndListen());
    }

    void onRequestReceived(
        nx_http::HttpServerConnection* const /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        m_urlsFromReceivedRequests.push(request.requestLine.url);
        completionHandler(nx_http::StatusCode::ok);
    }
};

TEST_F(
    AsyncHttpClientCorrectUrlTransferring,
    encoded_sequence_in_query_param_is_not_decoded)
{
    whenIssuedRequestWithEncodedSequenceInQueryAndFragment();
    assertServerHasReceivedCorrectUrl();
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

TEST_F(AsyncHttpClientTest, ReusingExistingConnection)
{
    static const char* testPath = "/ReusingExistingConnection";
    testHttpServer()->setPersistentConnectionEnabled(false);
    ASSERT_TRUE(
        testHttpServer()->registerStaticProcessor(
            testPath,
            "qwade324dwfasd123sdf23sdfsdf",
            "text/plain"));
    ASSERT_TRUE(testHttpServer()->bindAndListen());

    const QUrl testUrl(lm("http://%1%2")
        .str(testHttpServer()->serverAddress()).arg(testPath));

    for (int i = 0; i < 2; ++i)
    {
        nx::utils::SyncQueue<nx_http::Response> responseQueue;
        std::atomic<int> responseCount(0);

        auto httpClient = AsyncHttpClient::create();
        auto httpClientGuard = makeScopedGuard(
            [&httpClient]() { httpClient->pleaseStopSync(); });

        httpClient->setSendTimeoutMs(1000);
        QObject::connect(
            httpClient.get(), &AsyncHttpClient::done,
            [&testUrl, &responseQueue, &responseCount](AsyncHttpClientPtr client)
            {
                ++responseCount;
                if (client->response())
                    responseQueue.push(*client->response());
                else
                    responseQueue.push(nx_http::Response());
                if (responseCount >= 2)
                    return;
                client->doGet(testUrl);
            });
        if (i == 1)
        {
            // Testing connection to a bad url.
            httpClient->doGet(QUrl("http://example.com:58249/test"));
        }
        else
        {
            httpClient->doGet(testUrl);
        }

        constexpr const auto responseWaitDelay = std::chrono::seconds(3);
        ASSERT_TRUE((bool) responseQueue.pop(responseWaitDelay));
        ASSERT_TRUE((bool) responseQueue.pop(responseWaitDelay));
    }
}

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

TEST(AsyncHttpClientTest2, PartionedIncomingData)
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
