// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/chunked_body_source.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::http::test {

namespace {

class TestChunkedBodySource:
    public ChunkedBodySource
{
    using base_type = ChunkedBodySource;

public:
    using base_type::base_type;

    /**
     * Asserts on reading beyond the chunked stream EOF marker.
     */
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx::Buffer)
        > completionHandler) override
    {
        base_type::readAsync(
            [completionHandler = std::move(completionHandler)](
                SystemError::ErrorCode errorCode, nx::Buffer buf)
            {
                NX_ASSERT(!buf.empty());
                completionHandler(errorCode, std::move(buf));
            });
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

static constexpr char kTestPath[] = "/HttpServerConnectionTest/noContent";
static constexpr char kResourceWithBodyPath[] = "/HttpServerConnectionTest/resource";
static constexpr char kResourceWithChunkedBodyPath[] = "/HttpServerConnectionTest/chunked";

static constexpr char kResourceBody[] = "Hello, world";

static constexpr char kValidConnectionTestPath[] = "/HttpServerConnectionTest/validConnection";
static constexpr char kStreamingRequestBodyPath[] = "/HttpServerConnectionTest/streamingBody";

class HttpServerConnection:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        using namespace std::placeholders;

        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_httpServer.registerRequestProcessorFunc(
            kResourceWithBodyPath,
            std::bind(&HttpServerConnection::provideMessageBody, this, _1, _2));

        m_httpServer.registerRequestProcessorFunc(
            kResourceWithChunkedBodyPath,
            std::bind(&HttpServerConnection::provideChunkedMessageBody, this, _1, _2));

        m_httpServer.registerRequestProcessorFunc(
            kValidConnectionTestPath,
            [this](auto&&... args) { checkConnectionValidity(std::forward<decltype(args)>(args)...); });

        m_httpServer.registerRequestProcessorFunc(
            kStreamingRequestBodyPath,
            [this](auto&&... args) { saveStreamingBody(std::forward<decltype(args)>(args)...); },
            nx::network::http::kAnyMethod,
            MessageBodyDeliveryType::stream);
    }

    void whenRespondWithEmptyMessageBody()
    {
        installRequestHandlerWithEmptyBody();
        performRequest(prepareUrl(kUrlSchemeName, kTestPath));
    }

    void whenIssuedRequestViaSsl()
    {
        performRequest(prepareUrl(kSecureUrlSchemeName, kTestPath));
    }

    void whenIssuedRequestViaRawTcp()
    {
        performRequest(prepareUrl(kUrlSchemeName, kTestPath));
    }

    void whenReceiveStrictTransportSecurityHeaderInResponse()
    {
        whenIssuedRequestViaSsl();
        thenResponseContainsHeader(header::StrictTransportSecurity::NAME);
    }

    void whenReceivedChunkedResponse()
    {
        performRequest(prepareUrl(kUrlSchemeName, kResourceWithChunkedBodyPath));
    }

    void whenIssueRequestOverExistingConnection()
    {
        performRequest(
            prepareUrl(kUrlSchemeName, kResourceWithBodyPath),
            true /*reuseConnection*/);
    }

    void thenSuccessResponseIsReceived()
    {
        ASSERT_TRUE(m_prevResponse);
        ASSERT_EQ(StatusCode::ok, m_prevResponse->statusLine.statusCode);
    }

    void whenSendRequestAndCloseConnectionJustAfterSending()
    {
        sendRequestAndCloseConnectionJustAfterSending(
            kValidConnectionTestPath,
            256 * 1024);
    }

    void whenSendRequestWithIncompleteMessageBody()
    {
        sendRequestAndCloseConnectionJustAfterSending(
            kStreamingRequestBodyPath,
            256 * 1024,
            512 * 1024);
    }

    void whenInvokeRequestHandlerWithStreamingBody()
    {
        m_lastSentRequestBody = nx::utils::generateRandomName(256 * 1024);

        HttpClient client{ssl::kAcceptAnyCertificate};
        client.addAdditionalHeader("Connection", "close");
        ASSERT_TRUE(client.doPost(
            prepareUrl(kUrlSchemeName, kStreamingRequestBodyPath),
            "application/octet-stream",
            m_lastSentRequestBody));
    }

    void thenOnResponseSentHandlerIsCalled()
    {
        m_responseSentEvents.pop();
    }

    void thenResponseContainsHeader(const std::string& headerName)
    {
        ASSERT_TRUE(m_prevResponse->headers.find(headerName) != m_prevResponse->headers.end());
    }

    void thenResponseDoesNotContainHeader(const std::string& headerName)
    {
        ASSERT_TRUE(m_prevResponse->headers.find(headerName) == m_prevResponse->headers.end());
    }

    void thenStrictTransportSecurityHeaderIsValid()
    {
        const auto strictTransportSecurityStr = getHeaderValue(
            m_prevResponse->headers,
            header::StrictTransportSecurity::NAME);
        ASSERT_TRUE(m_prevStrictTransportSecurity.parse(strictTransportSecurityStr));
    }

    void andStrictTransportSecurityMaxAgeIsNotLessThan(
        std::chrono::seconds desiredMinimalAge)
    {
        ASSERT_GE(m_prevStrictTransportSecurity.maxAge, desiredMinimalAge);
    }

    void thenRequestIsReceived()
    {
        m_lastRequestReceived = m_requestsReceived.pop();
    }

    void andConnectionObjectIsValidInRequestHandler()
    {
        ASSERT_EQ(m_lastRequestOriginEndpoint, m_lastRequestReceived->clientEndpoint);
    }

    void thenRequestIsNotReceived()
    {
        ASSERT_FALSE(m_requestsReceived.pop(std::chrono::milliseconds(100)));
    }

    void andTheWholeMessageBodyIsRead()
    {
        const auto requestBody = readCompleteRequestBody();
        ASSERT_EQ(m_lastSentRequestBody, requestBody);
    }

    void thenRequestBodyEofIsReported()
    {
        // NOTE: the following call will block if no EOF is reported.
        readCompleteRequestBody();
    }

    void andTheConnectionIsReused()
    {
        ASSERT_GT(m_client->totalRequestsSentViaCurrentConnection(), 1);
    }

    TestHttpServer& httpServer()
    {
        return m_httpServer;
    }

    nx::utils::Url prepareUrl(const char* urlScheme, const char* path)
    {
        return nx::network::url::Builder()
            .setScheme(urlScheme).setEndpoint(m_httpServer.serverAddress()).setPath(path);
    }

private:
    struct LocalRequestContext
    {
        Request request;
        SocketAddress clientEndpoint;
    };

    TestHttpServer m_httpServer;
    SocketAddress m_lastRequestOriginEndpoint;
    nx::utils::SyncQueue<LocalRequestContext> m_requestsReceived;
    std::optional<LocalRequestContext> m_lastRequestReceived;
    nx::utils::SyncQueue<int /*dummy*/> m_responseSentEvents;
    std::optional<Response> m_prevResponse;
    header::StrictTransportSecurity m_prevStrictTransportSecurity;
    std::unique_ptr<HttpClient> m_client;
    nx::utils::SyncQueue<std::tuple<SystemError::ErrorCode, nx::Buffer>> m_messageBodyParts;
    nx::Buffer m_lastSentRequestBody;

    void installRequestHandlerWithEmptyBody()
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpServerConnection::provideEmptyMessageBody, this, _1, _2));
    }

    void performRequest(const nx::utils::Url& url, bool reuseConnection = false)
    {
        m_prevResponse = std::nullopt;

        if (!reuseConnection)
            m_client.reset();

        if (!m_client)
        {
            m_client = std::make_unique<HttpClient>(ssl::kAcceptAnyCertificate);
            m_client->setResponseReadTimeout(kNoTimeout);
            m_client->setMessageBodyReadTimeout(kNoTimeout);
        }

        ASSERT_TRUE(m_client->doGet(url));
        m_prevResponse = *m_client->response();

        auto body = m_client->fetchEntireMessageBody();
        if (body)
            m_prevResponse->messageBody = std::move(*body);
    }

    void provideEmptyMessageBody(
        RequestContext /*requestContext*/,
        RequestProcessedHandler completionHandler)
    {
        using namespace std::placeholders;

        RequestResult result(StatusCode::ok);
        result.connectionEvents.onResponseHasBeenSent =
            std::bind(&HttpServerConnection::onResponseSent, this, _1);
        result.dataSource = std::make_unique<EmptyMessageBodySource>(
            "text/plain",
            std::nullopt);
        completionHandler(std::move(result));
    }

    void provideMessageBody(
        RequestContext /*requestContext*/,
        RequestProcessedHandler completionHandler)
    {
        RequestResult result(StatusCode::ok);
        result.dataSource = std::make_unique<BufferSource>(
            "text/plain",
            kResourceBody);
        completionHandler(std::move(result));
    }

    void provideChunkedMessageBody(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler)
    {
        RequestResult result(StatusCode::ok);

        requestContext.response->headers.emplace("Transfer-Encoding", "chunked");
        result.dataSource = std::make_unique<TestChunkedBodySource>(
            std::make_unique<BufferSource>(
                "text/plain",
                kResourceBody));
        completionHandler(std::move(result));
    }

    void checkConnectionValidity(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler)
    {
        m_requestsReceived.push({
            requestContext.request,
            requestContext.clientEndpoint});

        completionHandler(StatusCode::ok);
    }

    void saveStreamingBody(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler)
    {
        m_requestsReceived.push({
            requestContext.request,
            requestContext.clientEndpoint});

        saveRequestBody(std::move(requestContext.body));

        completionHandler(StatusCode::ok);
    }

    void saveRequestBody(std::unique_ptr<AbstractMsgBodySourceWithCache> body)
    {
        auto bodyPtr = body.get();
        bodyPtr->readAsync(
            [this, body = std::move(body)](
                SystemError::ErrorCode resultCode, nx::Buffer buffer) mutable
            {
                const auto eof = resultCode != SystemError::noError || buffer.empty();

                m_messageBodyParts.push(std::make_tuple(resultCode, std::move(buffer)));
                if (!eof)
                    saveRequestBody(std::move(body));
                else
                    body.reset();
            });
    }

    void onResponseSent(nx::network::http::HttpServerConnection* /*connection*/)
    {
        m_responseSentEvents.push(0);
    }

    void sendRequestAndCloseConnectionJustAfterSending(
        const std::string_view& path,
        int bodySize,
        std::optional<int> contentLength = std::nullopt)
    {
        Request request;
        request.requestLine.method = Method::post;
        request.requestLine.url = path;
        request.requestLine.version = http_1_0;
        request.messageBody = nx::utils::generateRandomName(bodySize);
        request.headers.emplace(
            "Content-Length",
            std::to_string(contentLength ? *contentLength : bodySize));

        m_lastSentRequestBody = request.messageBody;

        sendRequestAndCloseConnectionJustAfterSending(request);
    }

    void sendRequestAndCloseConnectionJustAfterSending(const Request& request)
    {
        const auto serializedRequest = request.serialized();

        TCPSocket connection(AF_INET);
        ASSERT_TRUE(connection.connect(m_httpServer.serverAddress(), kNoTimeout));
        m_lastRequestOriginEndpoint = connection.getLocalAddress();
        ASSERT_EQ(
            serializedRequest.size(),
            connection.send(serializedRequest.data(), serializedRequest.size()));
    }

    nx::Buffer readCompleteRequestBody()
    {
        nx::Buffer result;
        for (;;)
        {
            auto bodyPart = m_messageBodyParts.pop();
            if (std::get<0>(bodyPart) != SystemError::noError || std::get<1>(bodyPart).empty())
                break;
            result += std::get<1>(bodyPart);
        }

        return result;
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(
    HttpServerConnection,
    on_response_sent_handler_is_called_after_response_with_empty_body)
{
    whenRespondWithEmptyMessageBody();
    thenOnResponseSentHandlerIsCalled();
}

TEST_F(
    HttpServerConnection,
    http_strict_transport_security_header_is_present_in_case_of_ssl_is_used)
{
    whenIssuedRequestViaSsl();
    thenResponseContainsHeader(header::StrictTransportSecurity::NAME);
}

TEST_F(
    HttpServerConnection,
    http_strict_transport_security_header_is_absent_in_case_of_ssl_is_used)
{
    whenIssuedRequestViaRawTcp();
    thenResponseDoesNotContainHeader(header::StrictTransportSecurity::NAME);
}

TEST_F(
    HttpServerConnection,
    http_strict_transport_security_header_is_valid)
{
    whenReceiveStrictTransportSecurityHeaderInResponse();

    thenStrictTransportSecurityHeaderIsValid();
    andStrictTransportSecurityMaxAgeIsNotLessThan(std::chrono::hours(24) * 365);
}

TEST_F(HttpServerConnection, connection_is_still_reusable_after_chunked_response)
{
    whenReceivedChunkedResponse();

    whenIssueRequestOverExistingConnection();
    thenSuccessResponseIsReceived();
    andTheConnectionIsReused();
}

TEST_F(HttpServerConnection, the_valid_connection_object_is_passed_to_the_request_handler)
{
    whenSendRequestAndCloseConnectionJustAfterSending();

    thenRequestIsReceived();
    andConnectionObjectIsValidInRequestHandler();
}

TEST_F(HttpServerConnection, streaming_request_body_is_reported)
{
    whenInvokeRequestHandlerWithStreamingBody();

    thenRequestIsReceived();
    andTheWholeMessageBodyIsRead();
}

TEST_F(HttpServerConnection, request_body_eof_is_reported)
{
    whenSendRequestWithIncompleteMessageBody();

    thenRequestBodyEofIsReported();
}

//-------------------------------------------------------------------------------------------------

class HttpServerConnectionClientEndpoint:
    public HttpServerConnection
{
    using base_type = HttpServerConnection;

protected:
    virtual void SetUp() override
    {
        using namespace std::placeholders;

        base_type::SetUp();

        httpServer().registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpServerConnectionClientEndpoint::saveHttpClientEndpoint, this,
                _1, _2));
    }

    void whenIssueRequest()
    {
        issueRequest({});

        m_httpClientEndpoint = m_client.socket()->getLocalAddress();
    }

    void whenIssueRequestWithXForwardedFor()
    {
        issueRequest({{ "X-Forwarded-For", "1.2.3.4" }});

        m_httpClientEndpoint =
            SocketAddress("1.2.3.4", m_client.socket()->getLocalAddress().port);
    }

    void whenIssueRequestWithXForwardedPort()
    {
        issueRequest({
            {"X-Forwarded-For", "1.2.3.4"},
            {"X-Forwarded-Port", "5831"}});

        m_httpClientEndpoint = SocketAddress("1.2.3.4", 5831);
    }

    void whenIssueRequestWithForwardedHeaderWithoutPort()
    {
        issueRequest({{"Forwarded", "for=1.2.3.4"}});

        m_httpClientEndpoint =
            SocketAddress("1.2.3.4", m_client.socket()->getLocalAddress().port);
    }

    void whenIssueRequestWithForwardedHeaderWithPort()
    {
        issueRequest({{"Forwarded", "for=1.2.3.4:5678"}});

        m_httpClientEndpoint = SocketAddress("1.2.3.4", 5678);
    }

    void thenHttpClientEndpointIsCorrect()
    {
        ASSERT_EQ(m_httpClientEndpoint, m_httpClientEndpoints.pop());
    }

private:
    nx::utils::SyncQueue<SocketAddress> m_httpClientEndpoints;
    SocketAddress m_httpClientEndpoint;
    HttpClient m_client{ssl::kAcceptAnyCertificate};

    void issueRequest(HttpHeaders additionalHeaders)
    {
        for (const auto& header: additionalHeaders)
            m_client.addAdditionalHeader(header.first, header.second);
        ASSERT_TRUE(m_client.doGet(prepareUrl(kUrlSchemeName, kTestPath)));
    }

    void saveHttpClientEndpoint(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler)
    {
        m_httpClientEndpoints.push(requestContext.clientEndpoint);

        completionHandler(StatusCode::ok);
    }
};

TEST_F(
    HttpServerConnectionClientEndpoint,
    if_no_special_header_present_than_tcp_connection_source_is_provided)
{
    whenIssueRequest();
    thenHttpClientEndpointIsCorrect();
}

TEST_F(HttpServerConnectionClientEndpoint, x_forwarded_for_header_is_used)
{
    whenIssueRequestWithXForwardedFor();
    thenHttpClientEndpointIsCorrect();
}

TEST_F(HttpServerConnectionClientEndpoint, x_forwarded_port_header_is_used)
{
    whenIssueRequestWithXForwardedPort();
    thenHttpClientEndpointIsCorrect();
}

TEST_F(HttpServerConnectionClientEndpoint, forwarded_header_without_port)
{
    whenIssueRequestWithForwardedHeaderWithoutPort();
    thenHttpClientEndpointIsCorrect();
}

TEST_F(HttpServerConnectionClientEndpoint, forwarded_header_with_port)
{
    whenIssueRequestWithForwardedHeaderWithPort();
    thenHttpClientEndpointIsCorrect();
}

//-------------------------------------------------------------------------------------------------

static constexpr char kPipeliningTestPath[] = "/HttpServerConnectionRequestPipelining";

class HttpServerConnectionRequestPipelining:
    public HttpServerConnection
{
     using base_type = HttpServerConnection;

public:
    ~HttpServerConnectionRequestPipelining()
    {
        if (m_clientConnection)
            m_clientConnection->pleaseStopSync();

        m_timer.pleaseStopSync();
    }

protected:
    void givenServerThatReordersResponses()
    {
        httpServer().registerRequestProcessorFunc(
            kPipeliningTestPath,
            [this](auto... args) { replyWhileReorderingResponses(std::move(args)...); });
    }

    void whenSendMultipleRequests()
    {
        const auto requestUrl = prepareUrl(kUrlSchemeName, kPipeliningTestPath);
        if (!m_clientConnection)
        {
            auto connection = std::make_unique<TCPSocket>(AF_INET);

            ASSERT_TRUE(connection->connect(url::getEndpoint(requestUrl), kNoTimeout))
                << SystemError::getLastOSErrorText();

            ASSERT_TRUE(connection->setNonBlockingMode(true))
                << SystemError::getLastOSErrorText();

            m_clientConnection = std::make_unique<http::AsyncMessagePipeline>(
                std::move(connection));
            m_clientConnection->setMessageHandler(
                [this](auto... args) { saveMessage(std::move(args)...); });
            m_clientConnection->startReadingConnection();
        }

        m_expectedResponseCount = 2;
        for (int i = 0; i < m_expectedResponseCount; ++i)
        {
            http::Message message(http::MessageType::request);
            message.request->requestLine.method = http::Method::get;
            message.request->requestLine.version = http::http_1_1;
            message.request->requestLine.url = requestUrl.path();
            message.request->headers.emplace("Sequence", std::to_string(i));

            m_clientConnection->sendMessage(std::move(message));
        }
    }

    void thenResponsesAreReceivedInRequestOrder()
    {
        std::optional<int> prevSequence;
        for (int i = 0; i < m_expectedResponseCount; ++i)
        {
            auto response = m_responseQueue.pop();
            ASSERT_NE(nullptr, response);

            const auto sequence = nx::utils::stoi(response->headers.find("Sequence")->second);
            if (prevSequence)
            {
                ASSERT_EQ(*prevSequence + 1, sequence);
            }

            prevSequence = sequence;
        }
    }

private:
    nx::utils::SyncQueue<std::unique_ptr<Response>> m_responseQueue;
    int m_expectedResponseCount = 0;
    std::unique_ptr<http::AsyncMessagePipeline> m_clientConnection;
    std::vector<nx::network::http::RequestProcessedHandler> m_postponedRequestCompletionHandlers;
    aio::Timer m_timer;

    void replyWhileReorderingResponses(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        const auto requestSequence = requestContext.request.headers.find("Sequence")->second;
        requestContext.response->headers.emplace(
            "Sequence",
            requestSequence);

        if (m_postponedRequestCompletionHandlers.empty())
        {
            m_postponedRequestCompletionHandlers.push_back(
                std::move(completionHandler));
        }
        else
        {
            completionHandler(StatusCode::ok);

            m_timer.start(
                std::chrono::milliseconds(10),
                [this]() { sendPostponedResponse(); });
        }
    }

    void sendPostponedResponse()
    {
        for (auto& handler: m_postponedRequestCompletionHandlers)
            handler(StatusCode::ok);
    }

    void saveMessage(http::Message message)
    {
        if (message.type == http::MessageType::response)
            m_responseQueue.push(std::make_unique<Response>(*message.response));
    }
};

TEST_F(HttpServerConnectionRequestPipelining, response_order_matches_request_order)
{
    givenServerThatReordersResponses();
    whenSendMultipleRequests();
    thenResponsesAreReceivedInRequestOrder();
}

} // namespace nx::network::http::test
