// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <variant>

#include <gtest/gtest.h>

#include <chrono>

#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/server/proxy/proxy_handler.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/writable_message_body.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/ssl/context.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/socket_delegate.h>
#include <nx/network/system_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/message_body.h>
#include <nx/network/test_support/synchronous_tcp_server.h>
#include <nx/network/aio/timer.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/uuid.h>

#include "repeating_buffer_msg_body_source.h"

namespace nx::network::http::test {

using namespace std::literals;

static constexpr char kTestPath[] = "/HttpAsyncClient_upgrade";
static constexpr char kUpgradeTo[] = "NXRELAY/0.1";
static constexpr char kNewProtocolMessage[] = "Hello, Http Client!";

namespace {

class BrokenSocket:
    public CustomStreamSocketDelegate<AbstractStreamSocket, TCPSocket>
{
    using base_type = CustomStreamSocketDelegate<AbstractStreamSocket, TCPSocket>;

public:
    BrokenSocket(SystemError::ErrorCode errToReport):
        base_type(&m_socket),
        m_socket(AF_INET),
        m_errToReport(errToReport)
    {
    }

    virtual bool setNonBlockingMode(bool /*val*/) override
    {
        SystemError::setLastErrorCode(m_errToReport);
        return false;
    }

private:
    TCPSocket m_socket;
    SystemError::ErrorCode m_errToReport = SystemError::noError;
};

} // namespace

//--------------------------------------------------------------------------------------------------

class UpgradableHttpServer:
    public nx::network::test::SynchronousStreamSocketServer
{
protected:
    virtual void processConnection(AbstractStreamSocket* connection) override
    {
        readRequest(connection);
        sendResponse(connection);
    }

private:
    void readRequest(AbstractStreamSocket* connection)
    {
        nx::Buffer buf;
        buf.resize(4096);

        connection->recv(buf.data(), buf.size());
    }

    void sendResponse(AbstractStreamSocket* connection)
    {
        // Sending response.
        static constexpr char responseMessage[] =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: NXRELAY/0.1\r\n"
            "Connection: Upgrade\r\n"
            "\r\n";

        auto response = nx::utils::buildString<nx::Buffer>(responseMessage, kNewProtocolMessage);

        ASSERT_EQ(
            response.size(),
            connection->send(response.data(), response.size()));
    }
};

//-------------------------------------------------------------------------------------------------

static constexpr char kExtraResponseHandlerPath[] = "/HttpClientTest/extraResponseMessage/";
static constexpr char kInfiniteResourcePath[] = "/HttpClientTest/infinite-resource";
static constexpr char kFiniteResourcePath[] = "/HttpClientTest/finite-resource";
static constexpr char kResourceLimitedByConnectionClosurePath[] = "/HttpClientTest/http-10-resource";
static constexpr char kEmptyResourceLimitedByConnectionClosurePath[] =
    "/HttpClientTest/http-10-empty-resource";
static constexpr char kMalformedResponsePath[] = "/HttpClientTest/malformed-response";
static constexpr char kPartialBodyResponsePath[] = "/HttpClientTest/partial-body-response";
static constexpr char kSaveRequestPath[] = "/HttpClientTest/save-request";
static constexpr auto kPartialBodyReadTimeout = 50ms;

static constexpr char kResponseBody[] = "testtesttest";

class HttpAsyncClient:
    public ::testing::Test
{
public:
    HttpAsyncClient():
        m_proxyHost(nx::format("%1.com").args(nx::Uuid::createUuid().toSimpleString()).toStdString())
    {
        m_credentials = Credentials("username", PasswordAuthToken("password"));

        createClient();
    }

    ~HttpAsyncClient()
    {
        if (m_client)
            m_client->pleaseStopSync();

        if (m_responseBodySource)
            m_responseBodySource->pleaseStopSync();
    }

protected:
    TestHttpServer m_httpServer;
    std::unique_ptr<AsyncClient> m_client;
    nx::utils::SyncQueue<RequestContext> m_receivedRequests;
    std::optional<RequestContext> m_lastRequestCtx;

    virtual void SetUp() override
    {
        m_httpServer.registerRequestProcessorFunc(
            "",
            [this](auto&&... args) { processHttpRequest(std::forward<decltype(args)>(args)...); });

        m_httpServer.registerRequestProcessorFunc(
            kExtraResponseHandlerPath,
            [this](auto&&... args)
            {
                handleExtraResponseMessageRequest(std::forward<decltype(args)>(args)...);
            });

        m_httpServer.registerRequestProcessorFunc(
            kInfiniteResourcePath,
            [this](auto&&... args)
            {
                handleInfiniteResourceRequest(std::forward<decltype(args)>(args)...);
            });

        m_httpServer.registerRequestProcessorFunc(
            kResourceLimitedByConnectionClosurePath,
            [this](auto&&... args)
            {
                sendBodyLimitedByConnectionClosure(std::forward<decltype(args)>(args)...);
            });

        m_httpServer.registerRequestProcessorFunc(
            kEmptyResourceLimitedByConnectionClosurePath,
            [this](auto&&... args)
            {
                sendEmptyBodyLimitedByConnectionClosure(
                    std::forward<decltype(args)>(args)...);
            });

        m_httpServer.registerRequestProcessorFunc(
            kMalformedResponsePath,
            [this](auto&&... args)
            {
                sendMalformedResponse(std::forward<decltype(args)>(args)...);
            });

        m_httpServer.registerStaticProcessor(
            kFiniteResourcePath,
            kResponseBody,
            "plain/text",
            Method::get);

        m_httpServer.registerRequestProcessorFunc(
            kPartialBodyResponsePath,
            [this](auto&&... args)
            {
                sendPartialBodyResponse(std::forward<decltype(args)>(args)...);
            });

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void sendRequest(const nx::utils::Url& url)
    {
        m_client->doGet(url, [this]() { saveResponse(); });
    }

    nx::utils::Url prepareUrl(const std::string& requestPath = std::string())
    {
        const auto serverAddress = m_synchronousServer
            ? m_synchronousServer->endpoint()
            : m_httpServer.serverAddress();
        return nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(serverAddress).setPath(requestPath);
    }

    void createClient()
    {
        if (m_client)
        {
            m_client->pleaseStopSync();
            m_client.reset();
        }

        m_client = std::make_unique<AsyncClient>(ssl::kAcceptAnyCertificate);
        configureClient();
    }

    void enableAuthentication()
    {
        m_httpServer.enableAuthentication(".*");
        m_httpServer.registerUserCredentials(m_credentials);
    }

    void disablePersistentServerConnections()
    {
        m_httpServer.server().setPersistentConnectionEnabled(false);
    }

    void givenSynchronousServer()
    {
        m_synchronousServer = std::make_unique<UpgradableHttpServer>();
        ASSERT_TRUE(m_synchronousServer->bindAndListen(SocketAddress::anyPrivateAddress));
        m_synchronousServer->start();
    }

    void givenClientWithPersistentConnectionToHttp11Server()
    {
        whenSendGetRequest();
        thenSuccessResponseIsReceived();
    }

    void givenHttpClientInitializedWithPreexistingConnectionToServer()
    {
        givenClientWithPersistentConnectionToHttp11Server();

        auto tcpConnection = m_client->executeInAioThreadSync(
            [this]()
            {
                auto connection = m_client->takeSocket();
                m_client.reset();
                return connection;
            });

        m_client = std::make_unique<AsyncClient>(
            std::move(tcpConnection), ssl::kAcceptAnyCertificate);
        configureClient();
    }

    // Returns error code that is reported by the socket in HTTP client.
    SystemError::ErrorCode givenHttpClientWithExternalMalfunctioningConnection()
    {
        m_client = std::make_unique<AsyncClient>(
            std::make_unique<BrokenSocket>(SystemError::invalidData),
            ssl::kAcceptAnyCertificate);
        configureClient();

        return SystemError::invalidData;
    }

    void givenFiniteResourceResponseBodySource()
    {
        initializeResourceResponseBodySource(
            kFiniteResourcePath);
    }

    void givenHttp10ResourceResponseBodySource()
    {
        initializeResourceResponseBodySource(
            kResourceLimitedByConnectionClosurePath);
    }

    void givenEstablishedSslConnectionToTheServer()
    {
        m_preestablishedConnection = std::make_unique<ssl::ClientStreamSocket>(
            ssl::Context::instance(),
            std::make_unique<TCPSocket>(AF_INET),
            ssl::kAcceptAnyCertificateCallback);

        ASSERT_TRUE(m_preestablishedConnection->connect(m_httpServer.serverAddress(), kNoTimeout));
    }

    void givenClientHasBodyReceptionTimeout()
    {
        m_client->setMessageBodyReadTimeout(kPartialBodyReadTimeout);
    }

    void givenClientStopsAndResumesReadingResponseBody()
    {
        m_client->setOnSomeMessageBodyAvailable(
            [this]()
            {
                m_client->stopReading();
                m_client->resumeReading();
            });
    }

    void whenSendGetRequest()
    {
        m_client->doGet(prepareUrl("/test"), [this]() { saveResponse(); });
    }

    void whenSendGetRequestProvidingPathOnly()
    {
        m_client->doGet("/test", [this]() { saveResponse(); });
    }

    void whenSendConnectRequest()
    {
        m_client->setProxyCredentials(m_credentials);
        m_client->doConnect(
            prepareUrl("/connect_test"),
            m_proxyHost,
            [this]() { saveResponse(); });
    }

    void whenSendRequestWithoutMessageBody(const std::string_view& method)
    {
        m_client->doRequest(
            method,
            prepareUrl("/request_wo_msg_body/"),
            [this]() { saveResponse(); });
    }

    void whenSendToBrokenHttpServerThatSendsExtraResponseMessages()
    {
        m_client->doGet(
            prepareUrl(kExtraResponseHandlerPath),
            [this]() { saveResponse(); });
    }

    void whenServerClosesConnection()
    {
        m_httpServer.server().closeAllConnections();
    }

    void whenRequestResourceWithInfiniteBody()
    {
        m_client->setOnSomeMessageBodyAvailable(
            [this]()
            {
                auto buf = m_client->fetchMessageBodyBuffer();
                if (!buf.empty())
                    m_responseBodyChunks.push({std::move(buf), ++m_bodyChunkSequence});
            });

        m_client->doGet(
            prepareUrl(kInfiniteResourcePath),
            [this]() { saveResponse(); });
    }

    void whenRequestResourceSignalledByClosingConnection()
    {
        m_expectedBody = kResponseBody;

        m_client->doGet(
            prepareUrl(kResourceLimitedByConnectionClosurePath),
            [this]() { saveResponse(); });
    }

    void whenRequestEmptyResourceSignalledByClosingConnection()
    {
        m_expectedBody.clear();

        m_client->doGet(
            prepareUrl(kEmptyResourceLimitedByConnectionClosurePath),
            [this]() { saveResponse(); });
    }

    void whenRequestMalformedResponseHandler()
    {
        m_client->doGet(
            prepareUrl(kMalformedResponsePath),
            [this]() { saveResponse(); });
    }

    void whenRequestPartialBodyResponseHandler()
    {
        m_expectedBody = kResponseBody;

        m_client->doGet(
            prepareUrl(kPartialBodyResponsePath),
            [this]() { saveResponse(); });
    }

    void whenSuspendHttpClient()
    {
        m_client->executeInAioThreadSync(
            [this]() { m_client->stopReading(); });
    }

    void whenResumeHttpClient()
    {
        m_client->executeInAioThreadSync(
            [this]() { m_client->resumeReading(); });
    }

    void whenStopAcceptingNewConnections()
    {
        // Stopping accepting new connections.
        m_httpServer.server().pleaseStopSync();
    }

    void whenReadBodyThroughStandaloneResponseBodySource()
    {
        m_prevRequestResult.body.clear();
        std::promise<bool /*success*/> done;

        m_responseBodySource->readAsync(
            [this, &done](auto&&... args)
            {
                saveBodyChunk(&done, std::forward<decltype(args)>(args)...);
            });

        ASSERT_TRUE(done.get_future().get());
    }

    void whenCreateClientThatReusesThePreestablishedConnection()
    {
        m_client = std::make_unique<http::AsyncClient>(
            std::exchange(m_preestablishedConnection, nullptr), ssl::kAcceptAnyCertificate);

        configureClient();
    }

    nx::Buffer whenPostRandomBody()
    {
        nx::Buffer body = nx::utils::random::generateName(32);
        m_client->doPost(
            prepareUrl(kSaveRequestPath),
            std::make_unique<BufferSource>("text/plain", body),
            [this]() { saveResponse(); });
        return body;
    }

    nx::Buffer whenPostRandomGzippedBody()
    {
        nx::Buffer body = nx::utils::random::generateName(32);
        m_client->addAdditionalHeader("Content-Encoding", "gzip");
        m_client->doPost(
            prepareUrl(kSaveRequestPath),
            std::make_unique<BufferSource>(
                "text/plain", nx::utils::bstream::gzip::Compressor::compressData(body)),
            [this]() { saveResponse(); });
        return body;
    }

    void thenExpectedBodyWasReceived(const nx::Buffer& expected)
    {
        const auto& ctx = thenRequestIsReceived();
        ASSERT_EQ(expected, ctx.request.messageBody);
    }

    void andRequestDidNotContainHeader(const std::string& name)
    {
        ASSERT_FALSE(lastRequestCtx().request.headers.contains(name));
    }

    void andRequestContainsHeader(const HttpHeader& header)
    {
        const auto [b, e] = lastRequestCtx().request.headers.equal_range(header.first);
        ASSERT_TRUE(std::any_of(b, e, [&header](const auto& val) { return val == header; }));
    }

    const RequestContext& thenRequestIsReceived()
    {
        m_lastRequestCtx = m_receivedRequests.pop();
        return *m_lastRequestCtx;
    }

    const RequestContext& lastRequestCtx() const
    {
        return *m_lastRequestCtx;
    }

    void thenCorrectConnectRequestIsReceived()
    {
        const auto& ctx = thenRequestIsReceived();

        ASSERT_EQ(nx::network::http::Method::connect, ctx.request.requestLine.method);
        ASSERT_EQ(m_proxyHost, ctx.request.requestLine.url.authority().toStdString());
    }

    void thenSuccessResponseIsReceived()
    {
        m_prevRequestResult = m_responses.pop();
        ASSERT_EQ(SystemError::noError, m_prevRequestResult.systemResultCode);
        ASSERT_EQ(StatusCode::ok, m_prevRequestResult.response->statusLine.statusCode);
    }

    void thenRequestHasFailed()
    {
        m_prevRequestResult = m_responses.pop();
        ASSERT_EQ(nullptr, m_prevRequestResult.response);
        ASSERT_TRUE(m_prevRequestResult.fail);
    }

    void thenResponseBodyReadingTimedOut()
    {
        m_prevRequestResult = m_responses.pop();
        ASSERT_NE(nullptr, m_prevRequestResult.response);
        ASSERT_EQ(StatusCode::ok, m_prevRequestResult.response->statusLine.statusCode);
        ASSERT_TRUE(m_prevRequestResult.fail);
        ASSERT_EQ(SystemError::timedOut, m_prevRequestResult.systemResultCode);
    }

    void thenRequestFailedAs(SystemError::ErrorCode expected)
    {
        m_prevRequestResult = m_responses.pop();
        ASSERT_EQ(expected, m_prevRequestResult.systemResultCode);
    }

    void thenResponseBodyIsReported()
    {
        m_responseBodyChunks.pop();
    }

    void thenResponseBodyIsNotReportedFor(std::chrono::milliseconds timeout)
    {
        const auto sequence = m_bodyChunkSequence.load();
        for (;;)
        {
            auto chunk = m_responseBodyChunks.pop(timeout);
            if (!chunk)
                return;

            ASSERT_LE(chunk->sequence, sequence);
        }
    }

    void andTheSameTcpConnectionHasBeenUsed()
    {
        std::optional<std::uint64_t> connectionId;
        while (!m_receivedRequests.empty())
        {
            auto requestConnectionId = m_receivedRequests.pop().connectionAttrs.id;
            if (connectionId)
                ASSERT_EQ(*connectionId, requestConnectionId);
            else
                connectionId = requestConnectionId;
        }
    }

    void andNoMoreEventsAreReported()
    {
        ASSERT_FALSE(m_responses.pop(std::chrono::milliseconds(10)));
    }

    void thenExpectedMessageBodyIsRead()
    {
        ASSERT_EQ(m_expectedBody, m_prevRequestResult.body);
    }

    void assertResponseBodySourceReportsValidContentLength()
    {
        ASSERT_TRUE(m_responseBodySource->contentLength());
        ASSERT_EQ(m_expectedBody.size(), *m_responseBodySource->contentLength());
    }

    void assertResponseBodySourceReportsNoContentLength()
    {
        ASSERT_FALSE(m_responseBodySource->contentLength());
    }

private:
    struct RequestResult
    {
        SystemError::ErrorCode systemResultCode = SystemError::noError;
        std::unique_ptr<nx::network::http::Response> response;
        nx::Buffer body;
        bool fail = false;
    };

    struct ResponseBodyChunk
    {
        nx::Buffer buf;
        int sequence = 0;
    };

    std::unique_ptr<UpgradableHttpServer> m_synchronousServer;
    std::string m_proxyHost;
    nx::utils::SyncQueue<RequestResult> m_responses;
    Credentials m_credentials;
    nx::utils::SyncQueue<ResponseBodyChunk> m_responseBodyChunks;
    std::atomic<int> m_bodyChunkSequence = 0;
    nx::Buffer m_expectedBody;
    RequestResult m_prevRequestResult;
    std::unique_ptr<AbstractMsgBodySource> m_responseBodySource;
    std::unique_ptr<AbstractStreamSocket> m_preestablishedConnection;

    void configureClient()
    {
        m_client->setResponseReadTimeout(kNoTimeout);
        m_client->setMessageBodyReadTimeout(kNoTimeout);
        m_client->setCredentials(m_credentials);
    }

    void processHttpRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_receivedRequests.push(std::move(requestContext));
        completionHandler(nx::network::http::StatusCode::ok);
    }

    void handleExtraResponseMessageRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        const auto authHeaderIter =
            requestContext.request.headers.find(header::Authorization::NAME);
        if (authHeaderIter != requestContext.request.headers.end())
        {
            if (header::DigestAuthorization authorizationHeader;
                authorizationHeader.parse(authHeaderIter->second) &&
                authorizationHeader.authScheme == header::AuthScheme::digest)
            {
                if (validateAuthorization(
                        requestContext.request.requestLine.method,
                        m_credentials,
                        authorizationHeader))
                {
                    completionHandler(http::StatusCode::ok);
                    return;
                }
            }
        }

        header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = header::AuthScheme::digest;
        wwwAuthenticate.params.emplace("nonce", "not_random_nonce");
        wwwAuthenticate.params.emplace("realm", "realm_of_possibility");
        wwwAuthenticate.params.emplace("algorithm", "MD5");

        http::RequestResult result(StatusCode::unauthorized);
        result.headers.emplace(
            header::WWWAuthenticate::NAME,
            wwwAuthenticate.serialized());

        result.headers.emplace("Connection", "close");

        result.body = std::make_unique<CustomLengthBody>(
            "text/plain",
            "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n",
            0);

        completionHandler(std::move(result));
    }

    void handleInfiniteResourceRequest(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        http::RequestResult result(StatusCode::ok);
        result.body = std::make_unique<RepeatingBufferMsgBodySource>(
            "text/plain",
            "infinite resource. ");
        completionHandler(std::move(result));
    }

    void sendBodyLimitedByConnectionClosure(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        auto body = std::make_unique<WritableMessageBody>("text/plain");
        body->writeBodyData(nx::Buffer(kResponseBody));
        body->writeEof();

        http::RequestResult result(StatusCode::ok);
        result.body = std::move(body);
        completionHandler(std::move(result));
    }

    void sendEmptyBodyLimitedByConnectionClosure(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        http::RequestResult result(StatusCode::ok);
        result.body =
            std::make_unique<EmptyMessageBodySource>("text/plain", std::nullopt);
        completionHandler(std::move(result));
    }

    void sendMalformedResponse(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        auto response = std::make_shared<nx::Buffer>(
            "GET /some HTTP/1.1\r\n"
            "\r\n");

        requestContext.conn.lock()->socket()->sendAsync(
            response.get(),
            [response, connection = requestContext.conn,
                completionHandler = std::move(completionHandler)](auto&&...)
            {
                connection.lock()->closeConnection(SystemError::interrupted);
                completionHandler(nx::network::http::StatusCode::internalServerError);
            });
    }

    void sendPartialBodyResponse(
        nx::network::http::RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        http::RequestResult result(StatusCode::ok);

        auto body = std::make_unique<WritableMessageBody>("text/plain");
        body->writeBodyData(nx::Buffer(kResponseBody));
        result.body = std::move(body);

        completionHandler(std::move(result));
    }

    void initializeResourceResponseBodySource(const char* path)
    {
        m_expectedBody = kResponseBody;

        m_client->setOnResponseReceived(
            [this]()
            {
                m_responseBodySource = m_client->takeResponseBodySource();
                saveResponse();
                m_client.reset();
            });

        m_client->doGet(prepareUrl(path), [this]() { saveResponse(); });

        thenSuccessResponseIsReceived();

        ASSERT_NE(nullptr, m_responseBodySource);
    }

    void saveResponse()
    {
        m_responses.push(RequestResult{
            m_client->lastSysErrorCode(),
            m_client->response()
                ? std::make_unique<Response>(*m_client->response())
                : nullptr,
            m_client->fetchMessageBodyBuffer(),
            m_client->failed()});
    }

    void saveBodyChunk(
        std::promise<bool /*success*/>* done,
        SystemError::ErrorCode resultCode,
        nx::Buffer buf)
    {
        if (resultCode != SystemError::noError)
            return done->set_value(false);
        if (buf.empty())
            return done->set_value(true);

        m_prevRequestResult.body += buf;
        m_responseBodySource->readAsync(
            [this, done](auto&&... args)
            {
                saveBodyChunk(done, std::forward<decltype(args)>(args)...);
            });
    }
};

TEST_F(HttpAsyncClient, connect_method)
{
    whenSendConnectRequest();
    thenCorrectConnectRequestIsReceived();
}

TEST_F(HttpAsyncClient, connect_with_authentication)
{
    enableAuthentication();

    whenSendConnectRequest();
    thenSuccessResponseIsReceived();
}

TEST_F(HttpAsyncClient, clears_receive_buffer_when_opening_new_connection)
{
    whenSendToBrokenHttpServerThatSendsExtraResponseMessages();
    thenSuccessResponseIsReceived();
}

TEST_F(HttpAsyncClient, post_without_message_body_is_properly_supported)
{
    whenSendRequestWithoutMessageBody(http::Method::post);
    thenSuccessResponseIsReceived();
}

TEST_F(HttpAsyncClient, put_without_message_body_is_properly_supported)
{
    whenSendRequestWithoutMessageBody(http::Method::put);
    thenSuccessResponseIsReceived();
}

TEST_F(HttpAsyncClient, reuses_persistent_connection)
{
    givenClientWithPersistentConnectionToHttp11Server();

    whenSendGetRequest();

    thenSuccessResponseIsReceived();
    andTheSameTcpConnectionHasBeenUsed();
}

TEST_F(HttpAsyncClient, silently_reconnects_if_inactive_persistent_connection_is_closed)
{
    givenClientWithPersistentConnectionToHttp11Server();

    whenServerClosesConnection();
    whenSendGetRequest();

    thenSuccessResponseIsReceived();
}

TEST_F(HttpAsyncClient, message_body_receival_can_be_stopped_and_resumed)
{
    whenRequestResourceWithInfiniteBody();
    thenResponseBodyIsReported();

    whenSuspendHttpClient();
    thenResponseBodyIsNotReportedFor(std::chrono::milliseconds(100));

    whenResumeHttpClient();
    thenResponseBodyIsReported();
}

TEST_F(HttpAsyncClient, requesting_over_externally_provided_connection)
{
    givenHttpClientInitializedWithPreexistingConnectionToServer();
    whenStopAcceptingNewConnections();

    for (int i = 0; i < 3; ++i)
    {
        // Verifying that external connection can be reused for multiple requests.
        whenSendGetRequestProvidingPathOnly();
        thenSuccessResponseIsReceived();
    }
}

// VMS-36204.
TEST_F(HttpAsyncClient, error_in_external_connection_is_handled_properly)
{
    const auto errCode = givenHttpClientWithExternalMalfunctioningConnection();
    whenSendGetRequestProvidingPathOnly();
    thenRequestFailedAs(errCode);
}

TEST_F(HttpAsyncClient, receives_message_body_with_end_signalled_by_closing_connection)
{
    whenRequestResourceSignalledByClosingConnection();

    thenSuccessResponseIsReceived();
    thenExpectedMessageBodyIsRead();
}

TEST_F(HttpAsyncClient, receives_empty_message_body_with_end_signalled_by_closing_connection)
{
    whenRequestEmptyResourceSignalledByClosingConnection();

    thenSuccessResponseIsReceived();
    thenExpectedMessageBodyIsRead();
}

TEST_F(HttpAsyncClient, finite_response_body_is_accessible_through_standalone_body_source)
{
    givenFiniteResourceResponseBodySource();
    assertResponseBodySourceReportsValidContentLength();

    whenReadBodyThroughStandaloneResponseBodySource();

    thenExpectedMessageBodyIsRead();
}

TEST_F(HttpAsyncClient, http10_response_body_is_accessible_through_standalone_body_source)
{
    givenHttp10ResourceResponseBodySource();
    assertResponseBodySourceReportsNoContentLength();

    whenReadBodyThroughStandaloneResponseBodySource();

    thenExpectedMessageBodyIsRead();
}

TEST_F(HttpAsyncClient, uses_existing_ssl_connection)
{
    givenEstablishedSslConnectionToTheServer();

    whenCreateClientThatReusesThePreestablishedConnection();
    whenSendGetRequest();

    thenSuccessResponseIsReceived();
}

TEST_F(HttpAsyncClient, malformed_response_message_is_processed_properly)
{
    whenRequestMalformedResponseHandler();

    thenRequestHasFailed();
    andNoMoreEventsAreReported();
}

TEST_F(HttpAsyncClient, body_reading_is_resumed_with_correct_timeout)
{
    givenClientHasBodyReceptionTimeout();
    givenClientStopsAndResumesReadingResponseBody();

    whenRequestPartialBodyResponseHandler();

    thenResponseBodyReadingTimedOut();
    thenExpectedMessageBodyIsRead();
}

TEST_F(HttpAsyncClient, upload_resource)
{
    const auto body = whenPostRandomBody();

    thenExpectedBodyWasReceived(body);
    andRequestDidNotContainHeader("Content-Encoding");
}

TEST_F(HttpAsyncClient, upload_gzipped_resource)
{
    const auto body = whenPostRandomGzippedBody();

    thenExpectedBodyWasReceived(body);
    andRequestContainsHeader({"Content-Encoding", "gzip"});
}

//-------------------------------------------------------------------------------------------------

class HttpAsyncClientConnectionUpgrade:
    public HttpAsyncClient
{
    using base_type = HttpAsyncClient;

protected:
    void initializeServerThatDoesNotSendUpgradeHeaderInResponse()
    {
        m_serverSendUpgradeHeaderInResponse = false;
    }

    void whenPerformedUpgrade()
    {
        m_client->doUpgrade(
            prepareUrl(kTestPath),
            kUpgradeTo,
            std::bind(&HttpAsyncClientConnectionUpgrade::onUpgradeDone, this));
    }

    void whenPerformedSuccessfulUpgrade()
    {
        whenPerformedUpgrade();
        auto responseContext = m_upgradeResponses.pop();
        assertUpgradeResponseIsCorrect(responseContext);
        m_upgradedConnection = std::move(responseContext.connection);
    }

    void thenUpgradeRequestIsCorrect()
    {
        const auto httpRequest = m_receivedRequests.pop().request;
        ASSERT_EQ(nx::network::http::Method::options, httpRequest.requestLine.method);
        ASSERT_EQ("Upgrade", nx::network::http::getHeaderValue(httpRequest.headers, "Connection"));
        ASSERT_EQ(kUpgradeTo, nx::network::http::getHeaderValue(httpRequest.headers, "Upgrade"));
    }

    void thenConnectionHasBeenUpgraded()
    {
        auto responseContext = m_upgradeResponses.pop();
        assertUpgradeResponseIsCorrect(responseContext);
    }

    void thenRequestHasFailed()
    {
        auto responseContext = m_upgradeResponses.pop();
        ASSERT_FALSE(responseContext.statusCode);
    }

    void thenTrafficSentByServerAfterResponseIsAvailableOnSocket()
    {
        nx::Buffer buf;
        buf.resize(sizeof(kNewProtocolMessage) - 1);
        const int bytesRead =
            m_upgradedConnection->recv(buf.data(), buf.size(), MSG_WAITALL);
        buf.resize(bytesRead);

        ASSERT_EQ(kNewProtocolMessage, buf);
    }

private:
    struct ResponseContext
    {
        std::optional<nx::network::http::StatusCode::Value> statusCode;
        std::unique_ptr<AbstractStreamSocket> connection;
    };

    nx::utils::SyncQueue<ResponseContext> m_upgradeResponses;
    bool m_serverSendUpgradeHeaderInResponse = true;
    std::unique_ptr<AbstractStreamSocket> m_upgradedConnection;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpAsyncClientConnectionUpgrade::processHttpRequest, this, _1, _2));

        base_type::SetUp();
    }

    void processHttpRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        http::RequestResult result(nx::network::http::StatusCode::switchingProtocols);

        if (m_serverSendUpgradeHeaderInResponse)
            result.headers.emplace("Upgrade", kUpgradeTo);
        else
            result.headers.emplace("Upgrade", std::string());
        result.headers.emplace("Connection", "Upgrade");

        m_receivedRequests.push(std::move(requestContext));

        completionHandler(std::move(result));
    }

    void onUpgradeDone()
    {
        ResponseContext response;
        response.connection = m_client->takeSocket();
        if (!m_client->failed())
        {
            response.statusCode = static_cast<nx::network::http::StatusCode::Value>(
                m_client->response()->statusLine.statusCode);
        }
        m_upgradeResponses.push(std::move(response));
    }

    void assertUpgradeResponseIsCorrect(
        const ResponseContext& responseContext)
    {
        ASSERT_EQ(nx::network::http::StatusCode::switchingProtocols, responseContext.statusCode);
        ASSERT_NE(nullptr, responseContext.connection);
    }
};

TEST_F(HttpAsyncClientConnectionUpgrade, upgrade_successful)
{
    whenPerformedUpgrade();
    thenUpgradeRequestIsCorrect();
    thenConnectionHasBeenUpgraded();
}

TEST_F(HttpAsyncClientConnectionUpgrade, upgrade_missing_protocol_in_response)
{
    initializeServerThatDoesNotSendUpgradeHeaderInResponse();

    whenPerformedUpgrade();
    thenRequestHasFailed();
}

TEST_F(HttpAsyncClientConnectionUpgrade, data_after_successfull_upgrade_is_not_lost)
{
    givenSynchronousServer();
    whenPerformedSuccessfulUpgrade();
    thenTrafficSentByServerAfterResponseIsAvailableOnSocket();
}

//-------------------------------------------------------------------------------------------------

TEST(HttpAsyncClientTypes, lexicalSerialization)
{
    ASSERT_EQ(nx::reflect::toString(AuthType::authBasicAndDigest), "authBasicAndDigest");
    ASSERT_EQ(nx::reflect::toString(AuthType::authBasic), "authBasic");
    ASSERT_EQ(nx::reflect::toString(AuthType::authDigest), "authDigest");

    ASSERT_EQ(nx::reflect::fromString<AuthType>("authBasicAndDigest"),
        AuthType::authBasicAndDigest);
    ASSERT_EQ(nx::reflect::fromString<AuthType>("authDigest"), AuthType::authDigest);
    ASSERT_EQ(nx::reflect::fromString<AuthType>("authBasic"), AuthType::authBasic);
}

//-------------------------------------------------------------------------------------------------

class HttpAsyncClientAuthorization:
    public HttpAsyncClient
{
    using base_type = HttpAsyncClient;

    static constexpr char kPath[] = "/HttpAsyncClientAuthorization/1";

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_credentials = Credentials("n", PasswordAuthToken("x"));
    }

    void givenServerWithDigestAuthentication()
    {
        m_httpServer.registerRequestProcessorFunc(
            kPath,
            [this](auto&&... args) { processRequest(std::forward<decltype(args)>(args)...); });
    }

    void whenIssueRequest()
    {
        createClient();
        m_client->setCredentials(m_credentials);

        sendRequest(prepareUrl(kPath));
    }

    void thenRequestSucceeded()
    {
        base_type::thenSuccessResponseIsReceived();

        m_lastClientAuthorization = m_authorizationReceived.pop();
    }

    void andAuthenticationWasPassedInOneStep()
    {
        ASSERT_EQ(0, m_lastClientAuthorization->requestNumberInConnection);
    }

    void andCachedServerNonceWasUsed()
    {
        ASSERT_TRUE(m_lastClientAuthorization->repeatedNonceIsUsed);
    }

    void andNonceCountInTheLastRequestWas(int expected)
    {
        auto ncIter = m_lastClientAuthorization->header.digest->params.find("nc");
        ASSERT_NE(m_lastClientAuthorization->header.digest->params.end(), ncIter);
        ASSERT_EQ(expected, nx::utils::stoi(ncIter->second, nullptr /*pos*/, 16));
    }

private:
    struct ClientAuthorization
    {
        header::Authorization header;
        unsigned int requestNumberInConnection = 0;
        bool repeatedNonceIsUsed = false;
    };

    Credentials m_credentials;
    nx::utils::SyncQueue<ClientAuthorization> m_authorizationReceived;
    std::map<std::string, int /*use count*/> m_generatedNonces;
    std::optional<ClientAuthorization> m_lastClientAuthorization;

private:
    void processRequest(
        RequestContext ctx,
        RequestProcessedHandler completionHandler)
    {
        const auto authzStr = getHeaderValue(ctx.request.headers, "Authorization");
        if (!authzStr.empty())
        {
            header::DigestAuthorization authorization;
            if (authorization.parse(authzStr) &&
                authorization.authScheme == header::AuthScheme::digest &&
                validateAuthorization(
                    ctx.request.requestLine.method,
                    m_credentials,
                    authorization))
            {
                const auto nonceUseCount =
                    ++m_generatedNonces[authorization.digest->params["nonce"]];

                m_authorizationReceived.push(ClientAuthorization{
                    authorization,
                    ctx.conn.lock()->connectionStatistics.messagesReceivedCount() - 1,
                    nonceUseCount > 1});
                return completionHandler(StatusCode::ok);
            }
        }

        http::RequestResult result(StatusCode::unauthorized);

        const auto nonce = nx::Uuid::createUuid().toSimpleStdString();
        m_generatedNonces.emplace(nonce, 0);

        header::WWWAuthenticate wwwAuthenticate;
        wwwAuthenticate.authScheme = header::AuthScheme::digest;
        wwwAuthenticate.params["nonce"] = nonce;
        wwwAuthenticate.params["realm"] = "nx";
        wwwAuthenticate.params["algorithm"] = "MD5";
        wwwAuthenticate.params["qop"] = "auth";
        result.headers.emplace(wwwAuthenticate.NAME, wwwAuthenticate.serialized());

        completionHandler(std::move(result));
    }
};

TEST_F(HttpAsyncClientAuthorization, server_nonce_is_cached_and_reused)
{
    givenServerWithDigestAuthentication();

    whenIssueRequest();
    thenRequestSucceeded();

    whenIssueRequest();
    thenRequestSucceeded();
    andAuthenticationWasPassedInOneStep();
    andCachedServerNonceWasUsed();
    andNonceCountInTheLastRequestWas(2);
}

//-------------------------------------------------------------------------------------------------

namespace {

class TransparentProxyHandler:
    public server::proxy::AbstractProxyHandler
{
public:
    TransparentProxyHandler(const SocketAddress& target):
        m_target(target)
    {
    }

protected:
    virtual void detectProxyTarget(
        const ConnectionAttrs&,
        const SocketAddress&,
        Request* const request,
        ProxyTargetDetectedHandler handler)
    {
        // Replacing the Host header with the proxy target as specified in [rfc7230; 5.4].
        request->headers.erase("Host");
        request->headers.emplace("Host", m_target.toString());

        handler(StatusCode::ok, m_target);
    }

private:
    const SocketAddress m_target;
};

} // namespace

class HttpAsyncClientProxy:
    public ::testing::Test
{
public:
    HttpAsyncClientProxy():
        m_proxy(server::Role::proxy),
        m_transparentProxy(server::Role::proxy)
    {
    }

    ~HttpAsyncClientProxy()
    {
        if (m_client)
            m_client->pleaseStopSync();
    }

protected:
    static constexpr char kResourcePath[] = "/HttpAsyncClientProxy/resource";
    static constexpr char kResource[] = "HttpAsyncClientProxy test resource";

    virtual void SetUp() override
    {
        m_resourceServer.registerStaticProcessor(
            kResourcePath,
            kResource,
            "text/plain");

        m_resourceServer.enableAuthentication(".*");

        m_userCredentials.username = "n";
        m_userCredentials.authToken = PasswordAuthToken("x");
        m_resourceServer.registerUserCredentials(m_userCredentials);

        ASSERT_TRUE(m_resourceServer.bindAndListen());

        // Launching proxy server.
        m_proxy.registerRequestProcessor<server::proxy::ProxyHandler>(kAnyPath);
        ASSERT_TRUE(m_proxy.bindAndListen());

        m_transparentProxy.registerRequestProcessor(
            kAnyPath,
            [this]()
            {
                return std::make_unique<TransparentProxyHandler>(
                    m_resourceServer.serverAddress());
            });
        ASSERT_TRUE(m_transparentProxy.bindAndListen());
    }

    void enableAuthorizationOnProxy()
    {
        m_proxyCredentials = Credentials();
        m_proxyCredentials->username = "p";
        m_proxyCredentials->authToken = PasswordAuthToken("h");

        m_proxy.enableAuthentication(".*");
        m_proxy.registerUserCredentials(*m_proxyCredentials);

        m_transparentProxy.enableAuthentication(".*");
        m_transparentProxy.registerUserCredentials(*m_proxyCredentials);
    }

    void whenRequestingResourceThroughProxy()
    {
        m_client = std::make_unique<AsyncClient>(ssl::kAcceptAnyCertificate);

        if (m_proxyCredentials)
            m_client->setProxyCredentials(*m_proxyCredentials);
        m_client->setProxyVia(m_proxy.serverAddress(), false, ssl::kAcceptAnyCertificate);

        m_client->setCredentials(m_userCredentials);
        m_client->doGet(
            getUrl(kResourcePath),
            [this]() { saveResponse(); });
    }

    void whenRequestingResourceThroughTransparentProxy()
    {
        m_client = std::make_unique<AsyncClient>(ssl::kAcceptAnyCertificate);
        m_client->setCredentials(m_userCredentials);
        if (m_proxyCredentials)
            m_client->setProxyCredentials(*m_proxyCredentials);

        m_client->doGet(
            url::Builder(getUrl(kResourcePath))
                .setEndpoint(m_transparentProxy.serverAddress()),
            [this]() { saveResponse(); });
    }

    nx::utils::Url getUrl(const std::string_view&)
    {
        return url::Builder().setScheme(kUrlSchemeName)
            .setEndpoint(m_resourceServer.serverAddress())
            .setPath(kResourcePath).toUrl();
    }

    void thenResourceIsDelivered()
    {
        auto result = m_responseQueue.pop();
        if (result.index() == 0)
            FAIL() << SystemError::toString(std::get<0>(result));

        auto response = std::get<Response>(result);
        ASSERT_EQ(StatusCode::ok, response.statusLine.statusCode);
        ASSERT_EQ(kResource, response.messageBody);
    }

private:
    nx::network::http::TestHttpServer m_resourceServer;
    nx::network::http::TestHttpServer m_proxy;
    nx::network::http::TestHttpServer m_transparentProxy;
    std::unique_ptr<AsyncClient> m_client;
    Credentials m_userCredentials;
    std::optional<Credentials> m_proxyCredentials;
    nx::utils::SyncQueue<std::variant<SystemError::ErrorCode, Response>> m_responseQueue;

private:
    void saveResponse()
    {
        if (m_client->failed())
            return m_responseQueue.push(m_client->lastSysErrorCode());

        auto response = *m_client->response();
        response.messageBody += m_client->fetchMessageBodyBuffer();
        m_responseQueue.push(std::move(response));
    }
};

TEST_F(HttpAsyncClientProxy, proxy_authorization_works)
{
    enableAuthorizationOnProxy();

    whenRequestingResourceThroughProxy();
    thenResourceIsDelivered();
}

// "Transparent proxying" is a proxying when client is not aware it is actually
// interacting with a proxy.
TEST_F(HttpAsyncClientProxy, authorization_on_a_transparent_proxy)
{
    enableAuthorizationOnProxy();

    whenRequestingResourceThroughTransparentProxy();
    thenResourceIsDelivered();
}

} // namespace nx::network::http::test
