#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

#include <gtest/gtest.h>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/thread.h>
#include <nx/utils/byte_stream/custom_output_stream.h>

#include "repeating_buffer_sender.h"

namespace nx {
namespace network {
namespace http {
namespace test {

namespace {

class TestMessageBody:
    public http::BufferSource
{
    using base_type = http::BufferSource;

public:
    TestMessageBody(
        const StringType& mimeType,
        BufferType msgBody,
        bool includeContentLength)
        :
        base_type(mimeType, std::move(msgBody)),
        m_includeContentLength(includeContentLength)
    {
    }

    virtual boost::optional<uint64_t> contentLength() const override
    {
        return m_includeContentLength ? base_type::contentLength() : boost::none;
    }

private:
    bool m_includeContentLength = true;
};

//-------------------------------------------------------------------------------------------------

class SslAssertHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    SslAssertHandler(bool expectSsl):
        m_expectSsl(expectSsl)
    {
    }

    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler)
    {
        EXPECT_EQ(m_expectSsl, requestContext.connection->isSsl());

        completionHandler(http::RequestResult(
            http::StatusCode::ok,
            std::make_unique<http::BufferSource>(
                http::BufferType("text/plain"), http::BufferType("ok"))));
    }

private:
    const bool m_expectSsl;
};

} // namespace

class HttpClientAsync:
    public ::testing::Test
{
public:
    HttpClientAsync():
        m_testHttpServer(std::make_unique<TestHttpServer>())
    {
    }

    TestHttpServer& testHttpServer()
    {
        return *m_testHttpServer;
    }

    const TestHttpServer& testHttpServer() const
    {
        return *m_testHttpServer;
    }

protected:
    std::unique_ptr<TestHttpServer> m_testHttpServer;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_testHttpServer->registerRequestProcessor<SslAssertHandler>(
            lit("/httpOnly"), []() { return std::make_unique<SslAssertHandler>(false); });

        m_testHttpServer->registerRequestProcessor<SslAssertHandler>(
            lit("/httpsOnly"), []() { return std::make_unique<SslAssertHandler>(true); });

        ASSERT_TRUE(
            m_testHttpServer->registerRequestProcessorFunc(
                "/saveRequest",
                std::bind(&HttpClientAsync::onRequestReceived, this, _1, _2)));
    }

    void whenPostSomeBodyWithContentLength()
    {
        postSomeBody(true);
    }

    void whenPostSomeBodyWithoutContentLength()
    {
        postSomeBody(false);
    }

    void thenRequestReceivedContainsContentLength()
    {
        auto request = m_receivedRequests.pop();
        ASSERT_TRUE(request.headers.find("Content-Length") != request.headers.end());
    }

    void thenRequestReceivedDoesNotContainContentLength()
    {
        auto request = m_receivedRequests.pop();
        ASSERT_TRUE(request.headers.find("Content-Length") == request.headers.end());
    }

    void doRequest(const QString& scheme, const std::string& path)
    {
        doRequest(url::Builder()
            .setScheme(scheme)
            .setEndpoint(m_testHttpServer->serverAddress())
            .setPath(path.c_str()));
    }

    void doRequest(const nx::utils::Url& url)
    {
        const auto client = std::make_unique<nx::network::http::AsyncClient>();
        auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });

        std::promise<bool /*hasRequestSucceeded*/> promise;
        client->doGet(url,
            [&promise, clienPtr = client.get()]()
            {
                 promise.set_value(clienPtr->hasRequestSucceeded());
            });

        ASSERT_TRUE(promise.get_future().get());
    }

    void testResult(const QString& path, const nx::network::http::BufferType& expectedResult)
    {
        const nx::utils::Url url(lit("http://%1%2")
            .arg(m_testHttpServer->serverAddress().toString()).arg(path));

        nx::utils::promise<void> promise;
        NX_INFO(this, lm("testResult: %1").arg(url));

        const auto client = std::make_unique<nx::network::http::AsyncClient>();
        auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });

        client->doGet(url,
            [&promise, &expectedResult, clienPtr = client.get()]()
            {
                 EXPECT_TRUE(clienPtr->hasRequestSucceeded());
                 EXPECT_EQ(expectedResult, clienPtr->fetchMessageBodyBuffer());
                 promise.set_value();
            });

        promise.get_future().wait();
    }

    static std::vector<QByteArray> partitionDataRandomly(const QByteArray& testData)
    {
        const int minChunkSize = testData.size() / 10;

        // Randomly partioning test data.
        std::vector<QByteArray> partitionedData;
        int bytesLeft = testData.size();
        int testDataPos = 0;
        while (bytesLeft > 0)
        {
            const int chunkSize = bytesLeft <= minChunkSize
                ? bytesLeft
                : nx::utils::random::number<int>(minChunkSize, bytesLeft);
            partitionedData.push_back(testData.mid(testDataPos, chunkSize));
            testDataPos += chunkSize;
            bytesLeft -= chunkSize;
        }

        return partitionedData;
    }

    static std::vector<QByteArray> partitionDataToPredefinedChunks(
        const QByteArray& testData,
        const std::vector<int>& chunkSizes)
    {
        NX_ASSERT(std::accumulate(chunkSizes.begin(), chunkSizes.end(), 0) == testData.size());

        std::vector<QByteArray> partitionedData;
        int currentPos = 0;
        for (const auto chunkSize : chunkSizes)
        {
            partitionedData.push_back(testData.mid(currentPos, chunkSize));
            currentPos += chunkSize;
        }
        return partitionedData;
    }

private:
    nx::utils::SyncQueue<nx::network::http::Request> m_receivedRequests;

    nx::utils::Url commonTestUrl(const QString& requestPath) const
    {
        return nx::network::url::Builder().setScheme("http")
            .setEndpoint(testHttpServer().serverAddress())
            .setPath(requestPath).toUrl();
    }

    void postSomeBody(bool includeContentLength)
    {
        const auto client = std::make_unique<nx::network::http::AsyncClient>();
        auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });

        client->setRequestBody(std::make_unique<TestMessageBody>(
            "plain/text",
            "Hello, world",
            includeContentLength));

        nx::utils::promise<void> promise;
        client->doPost(
            commonTestUrl("/saveRequest"),
            [&promise]() { promise.set_value(); });

        promise.get_future().wait();
    }

    void onRequestReceived(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_receivedRequests.push(std::move(requestContext.request));
        completionHandler(nx::network::http::StatusCode::ok);
    }
};

TEST_F(HttpClientAsync, Https)
{
    ASSERT_TRUE(m_testHttpServer->bindAndListen());
    doRequest(nx::network::http::kUrlSchemeName, "/httpOnly");
    doRequest(nx::network::http::kSecureUrlSchemeName, "/httpsOnly");
}

// TODO: #mux Better create HttpServer test and move it there.
TEST_F(HttpClientAsync, ServerModRewrite)
{
    m_testHttpServer->registerStaticProcessor(
        nx::network::http::kAnyPath, "root", "text/plain");
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
static void testHttpClientForFastRemove(const nx::utils::Url& url)
{
    // use different delays (10us - 0.5s) to catch problems on different stages
    for (uint time = 10; time < 500000; time *= 2)
    {
        const auto client = std::make_unique<nx::network::http::AsyncClient>();
        auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });

        client->doGet(url);

        // kill the client after some delay
        std::this_thread::sleep_for(std::chrono::microseconds(time));
    }
}
} // namespace

TEST_F(HttpClientAsync, FastRemove)
{
    testHttpClientForFastRemove(lit("http://127.0.0.1/"));
    testHttpClientForFastRemove(lit("http://localhost/"));
    testHttpClientForFastRemove(lit("http://doestNotExist.host/"));

    testHttpClientForFastRemove(lit("https://127.0.0.1/"));
    testHttpClientForFastRemove(lit("https://localhost/"));
    testHttpClientForFastRemove(lit("https://doestNotExist.host/"));
}

TEST_F(HttpClientAsync, FastRemoveBadHost)
{
    nx::utils::Url url(lit("http://doestNotExist.host/"));

    for (int i = 0; i < 99; ++i)
    {
        const auto client = std::make_unique<nx::network::http::AsyncClient>();
        auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });

        client->doGet(url);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

TEST_F(HttpClientAsync, motionJpegRetrieval)
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
    const nx::utils::Url url(lit("http://127.0.0.1:%1/mjpg").arg(m_testHttpServer->serverAddress().port));

    struct ClientContext
    {
        ~ClientContext()
        {
            client->pleaseStopSync();
            client.reset(); //ensuring client removed before multipartParser
        }

        std::unique_ptr<nx::network::http::AsyncClient> client;
        nx::network::http::MultipartContentParser multipartParser;
    };

    std::atomic<size_t> bytesProcessed(0);

    auto checkReceivedContentFunc = [&](const QnByteArrayConstRef& data) {
        ASSERT_EQ(data, frame1);
        bytesProcessed += data.size();
    };

    std::vector<ClientContext> clients(CONCURRENT_CLIENT_COUNT);
    for (ClientContext& clientCtx: clients)
    {
        clientCtx.client = std::make_unique<nx::network::http::AsyncClient>();
        clientCtx.multipartParser.setNextFilter(
            nx::utils::bstream::makeCustomOutputStream(checkReceivedContentFunc));

        clientCtx.client->setOnResponseReceived(
            [&clientCtx]()
            {
                ASSERT_NE(nullptr, clientCtx.client->response());
                ASSERT_EQ(StatusCode::ok, clientCtx.client->response()->statusLine.statusCode);
                auto contentTypeIter = clientCtx.client->response()->headers.find("Content-Type");
                ASSERT_TRUE(contentTypeIter != clientCtx.client->response()->headers.end());
                clientCtx.multipartParser.setContentType(contentTypeIter->second);
            });
        clientCtx.client->setOnSomeMessageBodyAvailable(
            [&clientCtx]()
            {
                clientCtx.multipartParser.processData(
                    clientCtx.client->fetchMessageBodyBuffer());
            });
        clientCtx.client->doGet(url);
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    clients.clear();
    m_testHttpServer.reset();
}

TEST_F(HttpClientAsync, posting_with_content_length)
{
    ASSERT_TRUE(testHttpServer().bindAndListen());

    whenPostSomeBodyWithContentLength();
    thenRequestReceivedContainsContentLength();
}

// TODO: #ak Revive test. It requires HTTP server with infinite message body support.
//TEST_F(HttpClientAsync, posting_without_content_length)
//{
//    ASSERT_TRUE(testHttpServer().bindAndListen());
//
//    whenPostSomeBodyWithoutContentLength();
//    thenRequestReceivedDoesNotContainContentLength();
//}

class HttpClientAsyncMultiRequest:
    public HttpClientAsync
{
public:
    HttpClientAsyncMultiRequest():
        m_client(std::make_unique<nx::network::http::AsyncClient>())
    {
        init();
    }

protected:
    void doRequest(const nx::utils::Url& url, const QByteArray& message)
    {
        m_expectedResponse = message;
        m_client->doGet(url);
        m_requestResultQueue.pop();
    }

    void doMultipleRequestsReusingHttpClient()
    {
        for (const auto& ctx: m_requests)
            doRequest(ctx.url, ctx.message);
    }

private:
    struct TestRequestContext
    {
        nx::utils::Url url;
        QByteArray message;
    };

    std::vector<TestRequestContext> m_requests;

    std::unique_ptr<nx::network::http::AsyncClient> m_client;
    nx::utils::SyncQueue<int /*dummy*/> m_requestResultQueue;
    QByteArray m_expectedResponse;

    void init()
    {
        const std::size_t testRequestCount = 3;
        m_requests.resize(testRequestCount);

        for (std::size_t i = 0; i < m_requests.size(); ++i)
        {
            m_requests[i].url = nx::utils::Url(
                lit("http://127.0.0.1/AsyncHttpClientTestMultiRequest_%1").arg(i));
            m_requests[i].message = lit("SimpleMessage_%1").arg(i).toLatin1();
        }

        for (const auto& ctx: m_requests)
        {
            ASSERT_TRUE(testHttpServer().registerStaticProcessor(
                ctx.url.path(),
                ctx.message,
                "application/text"));
        }

        ASSERT_TRUE(testHttpServer().bindAndListen());

        for (auto& ctx: m_requests)
            ctx.url.setPort(testHttpServer().serverAddress().port);

        m_client->setOnDone(
            [this]() { onDone(m_client.get()); });
    }

    void onDone(http::AsyncClient* client)
    {
        ASSERT_FALSE(client->failed()) << "Response: " <<
            (client->response() == nullptr
                ? std::string("null")
                : client->response()->statusLine.reasonPhrase.toStdString());
        client->socket()->cancelIOSync(nx::network::aio::etNone);

        ASSERT_EQ(nx::network::http::StatusCode::ok, client->response()->statusLine.statusCode);
        ASSERT_EQ(m_expectedResponse, client->fetchMessageBodyBuffer());
        auto contentTypeIter = client->response()->headers.find("Content-Type");
        ASSERT_TRUE(contentTypeIter != client->response()->headers.end());

        m_requestResultQueue.push(0 /*dummy*/);
    }
};

TEST_F(HttpClientAsyncMultiRequest, server_does_not_support_persistent_connections)
{
    testHttpServer().setPersistentConnectionEnabled(false);
    doMultipleRequestsReusingHttpClient();
}

TEST_F(HttpClientAsyncMultiRequest, server_supports_persistent_connections)
{
    testHttpServer().setPersistentConnectionEnabled(true);
    doMultipleRequestsReusingHttpClient();
}

class HttpClientAsyncCustom:
    public ::testing::Test
{
protected:
    void start(const QByteArray& response, bool breakAfterResponse = false)
    {
        nx::utils::promise<SocketAddress> address;
        serverThread = nx::utils::thread(
            [this, response, breakAfterResponse, &address]()
            {
                const auto server = std::make_unique<nx::network::TCPServerSocket>(
                    SocketFactory::tcpClientIpVersion());

                ASSERT_TRUE(server->bind(SocketAddress::anyPrivateAddress));
                ASSERT_TRUE(server->listen());
                NX_INFO(this, lm("Server address: %1").arg(server->getLocalAddress()));
                address.set_value(server->getLocalAddress());

                auto client = server->accept();
                ASSERT_TRUE((bool)client);

                do
                {
                    QByteArray buffer(1024, Qt::Uninitialized);
                    int size = 0;
                    while (!buffer.left(size).contains("\r\n\r\n"))
                    {
                        auto recv = client->recv(buffer.data() + size, buffer.size() - size);
                        if (recv == 0 && !breakAfterResponse)
                            return;

                        ASSERT_GT(recv, 0);
                        size += recv;
                    }

                    buffer.resize(size);
                    ASSERT_EQ(response.size(), client->send(response.data(), response.size()));
                }
                while (!breakAfterResponse);
            });

        serverAddress = address.get_future().get();
    }

    void testGet(
        nx::network::http::AsyncClient* client,
        const QByteArray& query,
        std::function<void(nx::network::http::AsyncClient*)> expectations,
        nx::network::http::HttpHeaders headers = {})
    {
        client->setAdditionalHeaders(headers);

        nx::utils::promise<void> clientDone;
        client->doGet(
            lit("http://%1/%2").arg(serverAddress.toString()).arg(QString::fromLatin1(query)),
            [client, &clientDone, &expectations]()
            {
                expectations(client);
                clientDone.set_value();
            });

        clientDone.get_future().wait();
    }

    ~HttpClientAsyncCustom()
    {
        serverThread.join();
    }

    nx::utils::thread serverThread;
    SocketAddress serverAddress;
};

TEST_F(HttpClientAsyncCustom, ConnectionBreak)
{
    static const QByteArray kResponse(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 100\r\n\r\n"
        "not enough content");

    start(kResponse, true);
    auto client = std::make_unique<nx::network::http::AsyncClient>();
    auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });

    testGet(client.get(), "test",
        [](nx::network::http::AsyncClient* client)
        {
            EXPECT_TRUE(client->failed());
            EXPECT_EQ(QByteArray("not enough content"), client->fetchMessageBodyBuffer());
            EXPECT_NE(SystemError::noError, client->lastSysErrorCode());
        });
}

TEST_F(HttpClientAsyncCustom, DISABLED_cameraThumbnail)
{
    static const QByteArray kResponseHeader =
        "HTTP/1.1 204 No Content\r\n"
        "Date: Wed, 12 Apr 2017 15:14:43 +0000\r\n"
        "Pragma: no-cache\r\n"
        "Server: Nx Witness/3.0.0.14559 (Network Optix) Apache/2.4.16 (Unix)\r\n"
        "Connection: Keep-Alive\r\n"
        "Keep-Alive: timeout=5\r\n"
        "Content-Type: application/ubjson\r\n"
        "Cache-Control: post-check=0, pre-check=0\r\n"
        "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
        "Content-Length: 46\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";

    static const QByteArray kResponseBody = QByteArray::fromHex(
        "5b6c000000035355244e6f20696d61676520666f756e6420666f722074686520676976656e20726571756573745d");

    static const QByteArray kRequestQuery =
        "ec2/cameraThumbnail?format=ubjson&cameraId=%7B38c5e727-b304-d188-0733-8619d09baae5%7D"
            "&time=latest&rotate=-1&height=184&widht=0&imageFormat=jpeg&method=after";

    static const nx::network::http::HttpHeaders kRequestHeaders =
    {
        {"X-server-guid", "{d16e21d4-0899-0a68-18c5-7e1058f229c4}"},
        {"X-Nx-User-Name", "admin"},
        {"X-runtime-guid", "{48c828fc-a821-462d-8015-49ae20b9936e}"},
        {"Accept-Encoding", "gzip"},
    };

    start(kResponseHeader + kResponseBody, false);
    auto client = std::make_unique<nx::network::http::AsyncClient>();
    auto clientGuard = nx::utils::makeScopeGuard([&client]() { client->pleaseStopSync(); });

    for (size_t i = 0; i < 5; ++i)
    {
        testGet(client.get(), kRequestQuery,
            [&](nx::network::http::AsyncClient* client)
            {
                // Response is invalid, it has a message body with code "204 No Content" what is
                // forbidden by HTTP RFC.
                EXPECT_TRUE(client->failed());
            },
            kRequestHeaders);
    }
}

namespace {
class TestHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    TestHandler(int requestNumber):
        m_mimeType("text/plain"),
        m_response("qweqweqwewqeqwe"),
        m_requestNumber(requestNumber)
    {
    }

    virtual void processRequest(
        http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        if (m_requestNumber > 0)
            requestContext.connection->takeSocket(); //< Closing connection by destroying socket.

        completionHandler(
            http::RequestResult(
                http::StatusCode::ok,
                std::make_unique<http::BufferSource >(m_mimeType, m_response)));
    }

private:
    const nx::network::http::StringType m_mimeType;
    const QByteArray m_response;
    int m_requestNumber;
};
} // namespace

TEST_F(HttpClientAsync, ConnectionBreakAfterReceivingSecondRequest)
{
    static const char* testPath = "/ConnectionBreakAfterReceivingSecondRequest";

    std::atomic<int> requestCounter(0);
    ASSERT_TRUE(
        testHttpServer().registerRequestProcessor<TestHandler>(
            testPath,
            [&requestCounter]() mutable
            {
                return std::make_unique<TestHandler>(requestCounter++);
            }));
    ASSERT_TRUE(testHttpServer().bindAndListen());

    nx::utils::Url testUrl(lit("http://%1%2")
        .arg(testHttpServer().serverAddress().toString())
        .arg(QString::fromLatin1(testPath)));

    nx::network::http::HttpClient httpClient;
    ASSERT_TRUE(httpClient.doGet(testUrl));
    ASSERT_NE(nullptr, httpClient.response());
    ASSERT_EQ(SystemError::noError, httpClient.lastSysErrorCode());
    ASSERT_FALSE(httpClient.doGet(testUrl));
    ASSERT_NE(SystemError::noError, httpClient.lastSysErrorCode());
}

//-------------------------------------------------------------------------------------------------
// HttpClientAsyncRequestValidation

class HttpClientAsyncRequestValidation:
    public HttpClientAsync
{
public:
    HttpClientAsyncRequestValidation()
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
        const auto query = QUrl::toPercentEncoding("param1=test#%20#&param2");
        const auto fragment = QUrl::toPercentEncoding("#frag%20ment");

        m_testUrl = nx::utils::Url(lit("http://%1%2?%3#%4")
            .arg(testHttpServer().serverAddress().toString())
            .arg(testPath()).arg(QLatin1String(query)).arg(QLatin1String(fragment)));

        ASSERT_TRUE(m_httpClient.doGet(m_testUrl));
    }

    void whenSendingRequest()
    {
        ASSERT_TRUE(m_httpClient.doGet(commonTestUrl()));
    }

    void thenHostHeaderContainsServerEndpoint()
    {
        const auto request = m_receivedRequests.pop();
        ASSERT_EQ(
            testHttpServer().serverAddress().toString().toUtf8(),
            nx::network::http::getHeaderValue(request.headers, "Host"));
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
    nx::utils::SyncQueue<nx::network::http::Request> m_receivedRequests;
    nx::utils::SyncQueue<nx::utils::Url> m_urlsFromReceivedRequests;
    nx::network::http::HttpClient m_httpClient;
    nx::utils::Url m_testUrl;

    void init()
    {
        using namespace std::placeholders;

        ASSERT_TRUE(
            testHttpServer().registerRequestProcessorFunc(
                testPath(),
                std::bind(&HttpClientAsyncRequestValidation::onRequestReceived, this,
                    _1, _2)));

        ASSERT_TRUE(testHttpServer().bindAndListen());
    }

    nx::utils::Url commonTestUrl() const
    {
        return nx::utils::Url(lm("http://%1%2").arg(testHttpServer().serverAddress()).arg(testPath()));
    }

    void onRequestReceived(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_urlsFromReceivedRequests.push(requestContext.request.requestLine.url);
        m_receivedRequests.push(std::move(requestContext.request));
        completionHandler(nx::network::http::StatusCode::ok);
    }
};

TEST_F(
    HttpClientAsyncRequestValidation,
    encoded_sequence_in_url_query_param_is_not_decoded)
{
    whenIssuedRequestWithEncodedSequenceInQueryAndFragment();
    assertServerHasReceivedCorrectUrl();
}

TEST_F(HttpClientAsyncRequestValidation, host_header_contains_endpoint)
{
    whenSendingRequest();
    thenHostHeaderContainsServerEndpoint();
}

//-------------------------------------------------------------------------------------------------

class HttpClientAsyncReliability:
    public ::testing::Test
{
public:
    constexpr static int kNumberOfClients = 100;

    HttpClientAsyncReliability():
        m_testServer(false, nx::network::NatTraversalSupport::disabled)
    {
    }

    virtual ~HttpClientAsyncReliability() override
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
            clientContext.client = std::make_unique<nx::network::http::AsyncClient>();
            clientContext.client->setResponseReadTimeout(std::chrono::milliseconds(500));
            clientContext.client->setMessageBodyReadTimeout(std::chrono::milliseconds(500));
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
        std::unique_ptr<nx::network::http::AsyncClient> client;
        nx::utils::promise<void> requestDonePromise;
        int requestsToSend;
    };

    RandomlyFailingHttpServer m_testServer;
    SocketAddress m_serverEndpoint;
    std::vector<ClientContext> m_clients;

    nx::utils::Url serverUrl() const
    {
        return nx::utils::Url(lit("http://%1/tst").arg(m_serverEndpoint.toString()));
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
            std::bind(&HttpClientAsyncReliability::onRequestDone, this, clientContext));
    }

    void waitForEachRequestCompletion()
    {
        for (auto& clientContext: m_clients)
            clientContext.requestDonePromise.get_future().wait();
    }

    void onRequestDone(ClientContext* clientContext)
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

TEST_F(HttpClientAsyncReliability, BreakingConnectionAtRandomPoint)
{
    givenRandomlyFailingServer();
    givenMultipleHttpClients();
    whenEachClientHasPerformedRequestToTheServer();
    thenEachClientShouldHaveCorrectState();
}

//-------------------------------------------------------------------------------------------------
// HttpClientAsyncReusingConnection

class HttpClientAsyncReusingConnection:
    public HttpClientAsync
{
public:
    static const char* testPath;

    HttpClientAsyncReusingConnection()
    {
        m_httpClient = std::make_unique<nx::network::http::AsyncClient>();
        m_httpClient->setSendTimeout(std::chrono::seconds(1));
        m_httpClient->setOnDone(
            [this]() { onRequestCompleted(m_httpClient.get()); });
    }

    ~HttpClientAsyncReusingConnection()
    {
        m_httpClient->pleaseStopSync();
        m_testHttpServer.reset();
    }

protected:
    void initializeStaticContentServer()
    {
        testHttpServer().setPersistentConnectionEnabled(false);
        NX_GTEST_ASSERT_TRUE(
            testHttpServer().registerStaticProcessor(
                testPath,
                "qwade324dwfasd123sdf23sdfsdf",
                "text/plain"));
        NX_GTEST_ASSERT_TRUE(testHttpServer().bindAndListen());

        m_testUrl = lm("http://%1%2").arg(testHttpServer().serverAddress()).arg(testPath);
    }

    void initializeDelayedConnectionClosureServer()
    {
        using namespace std::placeholders;

        testHttpServer().setPersistentConnectionEnabled(true);

        auto httpHandlerFunc =
            std::bind(&HttpClientAsyncReusingConnection::delayedConnectionClosureHttpHandlerFunc,
                this, _1, _2);
        NX_GTEST_ASSERT_TRUE(
            testHttpServer().registerRequestProcessorFunc(
                testPath, std::move(httpHandlerFunc)));
        NX_GTEST_ASSERT_TRUE(testHttpServer().bindAndListen());

        m_testUrl = lm("http://%1%2").arg(testHttpServer().serverAddress()).arg(testPath);
    }

    void scheduleRequestToValidUrlJustAfterFirstRequest()
    {
        m_scheduledRequests.push(m_testUrl);
    }

    void performRequestToValidUrl()
    {
        m_httpClient->doGet(m_testUrl);
    }

    void performRequestToInvalidUrl()
    {
        m_httpClient->doGet(nx::utils::Url("http://example.com:58249/test"));
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

    nx::utils::Url m_testUrl;
    nx::utils::SyncQueue<std::unique_ptr<nx::network::http::Response>> m_responseQueue;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    std::queue<nx::utils::Url> m_scheduledRequests;
    std::map<
        nx::network::http::HttpServerConnection*,
        std::unique_ptr<HttpConnectionContext>> m_connectionToContext;
    QnMutex m_mutex;

    void sendNextRequest()
    {
        const auto scheduledRequestUrl = m_scheduledRequests.front();
        m_scheduledRequests.pop();
        m_httpClient->doGet(scheduledRequestUrl);
    }

    void onRequestCompleted(http::AsyncClient* client)
    {
        if (client->response() && !client->failed())
            m_responseQueue.push(std::make_unique<nx::network::http::Response>(*client->response()));
        else
            m_responseQueue.push(nullptr);

        if (m_scheduledRequests.empty())
            return;

        sendNextRequest();
    }

    void delayedConnectionClosureHttpHandlerFunc(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        nx::network::http::RequestResult requestResult(nx::network::http::StatusCode::ok);

        HttpConnectionContext* connectionContext = nullptr;
        {
            QnMutexLocker lock(&m_mutex);
            auto p = m_connectionToContext.emplace(requestContext.connection, nullptr);
            if (p.second)
            {
                p.first->second = std::make_unique<HttpConnectionContext>();
                requestContext.connection->registerCloseHandler(
                    std::bind(&HttpClientAsyncReusingConnection::onConnectionClosed, this, requestContext.connection));
            }
            connectionContext = p.first->second.get();
        }

        ++connectionContext->requestsReceived;

        requestResult.connectionEvents.onResponseHasBeenSent =
            std::bind(&HttpClientAsyncReusingConnection::onResponseSent, this, requestContext.connection);

        completionHandler(std::move(requestResult));
    }

    void onResponseSent(nx::network::http::HttpServerConnection* connection)
    {
        QnMutexLocker lock(&m_mutex);

        auto it = m_connectionToContext.find(connection);
        if (it == m_connectionToContext.end())
            return;

        it->second->timer.bindToAioThread(connection->getAioThread());
        it->second->timer.start(
            std::chrono::milliseconds(nx::utils::random::number<int>(1, 30)),
            std::bind(&HttpClientAsyncReusingConnection::closeConnection, this, connection));
    }

    void closeConnection(nx::network::http::HttpServerConnection* connection)
    {
        connection->socket()->shutdown();
        connection->closeConnection(SystemError::connectionReset);
    }

    void onConnectionClosed(nx::network::http::HttpServerConnection* connection)
    {
        QnMutexLocker lock(&m_mutex);

        auto it = m_connectionToContext.find(connection);
        if (it != m_connectionToContext.end())
            m_connectionToContext.erase(it);
    }
};

const char* HttpClientAsyncReusingConnection::testPath = "/ReusingExistingConnection";

TEST_F(HttpClientAsyncReusingConnection, connecting_to_the_valid_url_first)
{
    initializeStaticContentServer();
    scheduleRequestToValidUrlJustAfterFirstRequest();
    performRequestToValidUrl();
    assertRequestSucceeded();
    assertRequestSucceeded();
}

TEST_F(HttpClientAsyncReusingConnection, connecting_to_the_invalid_url_first)
{
    initializeStaticContentServer();
    scheduleRequestToValidUrlJustAfterFirstRequest();
    performRequestToInvalidUrl();
    assertRequestFailed();
    assertRequestSucceeded();
}

TEST_F(HttpClientAsyncReusingConnection, server_closes_connection_with_random_delay)
{
    initializeDelayedConnectionClosureServer();
    scheduleRequestToValidUrlJustAfterFirstRequest();
    performRequestToValidUrl();
    assertRequestSucceeded();
    assertRequestSucceeded();
}

//-------------------------------------------------------------------------------------------------

class TestTcpServer:
    public nx::utils::Thread
{
public:
    TestTcpServer(std::vector<QByteArray> dataToSend):
        m_delayAfterSend(200),
        m_dataToSend(std::move(dataToSend)),
        m_serverSocket(SocketFactory::tcpClientIpVersion())
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
        constexpr const std::chrono::milliseconds acceptTimeout =
            std::chrono::milliseconds(1);

        m_serverSocket.setRecvTimeout(acceptTimeout.count());
        while (!m_terminated)
        {
            auto connection = m_serverSocket.accept();
            if (!connection)
                continue;

            for (const auto& buf: m_dataToSend)
            {
                if (connection->send(buf) != buf.size())
                    break; //< Client could close connection.
            }

            if (m_keepConnection)
                m_connections.push_back(std::move(connection));
        }
    }

private:
    const std::chrono::milliseconds m_delayAfterSend;
    std::vector<QByteArray> m_dataToSend;
    nx::network::TCPServerSocket m_serverSocket;
    bool m_terminated = false;
    const bool m_keepConnection = true;
    std::vector<std::unique_ptr<AbstractStreamSocket>> m_connections;
};

TEST_F(HttpClientAsync, PartionedIncomingData)
{
    const QByteArray testData(R"http(
HTTP/1.1 401 Unauthorized
Date: Wed, 19 Oct 2016 13:24:10 +0000
Server: Nx Witness/3.0.0.13295 (Network Optix) Apache/2.4.16 (Unix)
Content-Type: application/json
Content-Length: 103
WWW-Authenticate: Digest algorithm="MD5", nonce="15185146660140348415", realm="VMS"
X-Nx-Result-Code: notAuthorized

{"errorClass":"unauthorized","errorDetail":100,"errorText":"unauthorized","resultCode":"notAuthorized"}HTTP/1.1 200 OK
Date: Wed, 19 Oct 2016 13:24:10 +0000
Server: Nx Witness/3.0.0.13295 (Network Optix) Apache/2.4.16 (Unix)
Content-Type: application/json
Content-Length: 103
X-Nx-Result-Code: ok

{"errorClass":"unauthorized","errorDetail":100,"errorText":"unauthorized","resultCode":"notAuthorized"}
)http");

    auto dataToSend = partitionDataRandomly(testData);

    TestTcpServer testTcpServer(dataToSend);
    ASSERT_TRUE(testTcpServer.bindAndListen())
        << SystemError::getLastOSErrorText().toStdString();
    testTcpServer.start();

    nx::utils::Url url(lit("http://%1/secret/path").arg(testTcpServer.endpoint().toString()));
    url.setUserName("don't tell");
    url.setPassword("anyone");
    nx::network::http::HttpClient httpClient;
    httpClient.setResponseReadTimeout(nx::network::kNoTimeout);
    ASSERT_TRUE(httpClient.doGet(url));
    ASSERT_NE(nullptr, httpClient.response());
    ASSERT_EQ(nx::network::http::StatusCode::ok, httpClient.response()->statusLine.statusCode);
}

} // namespace test
} // namespace http
} // namespace network
} // namespace nx
