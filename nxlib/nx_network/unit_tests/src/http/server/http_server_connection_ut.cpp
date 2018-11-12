#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace http {
namespace test {

static const char* const kTestPath = "/HttpServerConnectionTest";

class HttpServerConnection:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void whenRespondWithEmptyMessageBody()
    {
        installRequestHandlerWithEmptyBody();
        performRequest(kUrlSchemeName);
    }

    void whenIssuedRequestViaSsl()
    {
        performRequest(kSecureUrlSchemeName);
    }

    void whenIssuedRequestViaRawTcp()
    {
        performRequest(kUrlSchemeName);
    }

    void whenReceiveStrictTransportSecurityHeaderInResponse()
    {
        whenIssuedRequestViaSsl();
        thenResponseContainsHeader(header::StrictTransportSecurity::NAME);
    }

    void thenOnResponseSentHandlerIsCalled()
    {
        m_responseSentEvents.pop();
    }

    void thenResponseContainsHeader(const nx::String& headerName)
    {
        ASSERT_TRUE(m_prevResponse->headers.find(headerName) != m_prevResponse->headers.end());
    }

    void thenResponseDoesNotContainHeader(const nx::String& headerName)
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

    TestHttpServer& httpServer()
    {
        return m_httpServer;
    }

    nx::utils::Url prepareRequestUrl(const char* requestPath) const
    {
        return nx::network::url::Builder().setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress()).setPath(requestPath);
    }

private:
    TestHttpServer m_httpServer;
    nx::utils::SyncQueue<int /*dummy*/> m_responseSentEvents;
    boost::optional<Response> m_prevResponse;
    header::StrictTransportSecurity m_prevStrictTransportSecurity;

    void installRequestHandlerWithEmptyBody()
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            kTestPath,
            std::bind(&HttpServerConnection::provideEmptyMessageBody, this, _1, _2));
    }

    void performRequest(const char* urlScheme)
    {
        HttpClient client;
        const auto url = nx::network::url::Builder()
            .setScheme(urlScheme).setEndpoint(m_httpServer.serverAddress()).setPath(kTestPath);
        ASSERT_TRUE(client.doGet(url));
        m_prevResponse = *client.response();
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
            boost::none);
        completionHandler(std::move(result));
    }

    void onResponseSent(nx::network::http::HttpServerConnection* /*connection*/)
    {
        m_responseSentEvents.push(0);
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
    HttpClient m_client;

    void issueRequest(HttpHeaders additionalHeaders)
    {
        for (const auto& header: additionalHeaders)
            m_client.addAdditionalHeader(header.first, header.second);
        ASSERT_TRUE(m_client.doGet(prepareRequestUrl(kTestPath)));
    }

    void saveHttpClientEndpoint(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler)
    {
        m_httpClientEndpoints.push(requestContext.connection->clientEndpoint());

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
        const auto requestUrl = prepareRequestUrl(kPipeliningTestPath);
        if (!m_clientConnection)
        {
            auto connection = std::make_unique<TCPSocket>(AF_INET);
            
            ASSERT_TRUE(connection->connect(url::getEndpoint(requestUrl), kNoTimeout))
                << SystemError::getLastOSErrorText().toStdString();
            
            ASSERT_TRUE(connection->setNonBlockingMode(true))
                << SystemError::getLastOSErrorText().toStdString();

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
            message.request->headers.emplace("Sequence", std::to_string(i).c_str());

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

            const auto sequence = response->headers.find("Sequence")->second.toInt();
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

} // namespace test
} // namespace nx
} // namespace network
} // namespace http
