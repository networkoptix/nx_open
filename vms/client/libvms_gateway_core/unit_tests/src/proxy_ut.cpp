// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_setup.h"

#include <optional>

#include <QtCore/QUrlQuery>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/chunked_transfer_encoder.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/http_stream_reader.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

using namespace nx::network::http;

static constexpr char testPath[] = "/test";
static constexpr char testQuery[] = "testQuery";
static const std::string testPathAndQuery(nx::format("%1?%2").args(testPath, testQuery).toStdString());
static const std::string checkuedTestPathAndQuery(nx::format("%1?%2&chunked").args(testPath, testQuery).toStdString());
static constexpr char testMsgBody[] = "bla-bla-bla";
static constexpr char testMsgContentType[] = "text/plain";

class UndefinedContentLengthBufferSource: public nx::network::http::BufferSource
{
public:
    UndefinedContentLengthBufferSource(): nx::network::http::BufferSource(testMsgContentType, testMsgBody) {}
    virtual std::optional<uint64_t> contentLength() const override { return std::nullopt; }
};

class VmsGatewayProxyTestHandler:
    public nx::network::http::RequestHandlerWithContext
{
public:
    VmsGatewayProxyTestHandler(std::optional<bool> securityExpectation):
        m_securityExpectation(securityExpectation)
    {
    }

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler )
    {
        using namespace nx::network;

        if (m_securityExpectation)
        {
            EXPECT_EQ(*m_securityExpectation, requestContext.connectionAttrs.isSsl);
        }

        QUrlQuery requestQuery(requestContext.request.requestLine.url.query());

        if (requestContext.request.requestLine.url.path() == testPath &&
            requestQuery.hasQueryItem(testQuery))
        {
            http::RequestResult result(http::StatusCode::ok);
            if (requestQuery.hasQueryItem("chunked"))
            {
                result.headers.emplace("Transfer-Encoding", "chunked");
                result.body = std::make_unique<http::BufferSource>(testMsgContentType,
                    http::QnChunkedTransferEncoder::serializeSingleChunk(testMsgBody)+"0\r\n\r\n");
            }
            else if (requestQuery.hasQueryItem("undefinedContentLength"))
            {
                result.body = std::make_unique<UndefinedContentLengthBufferSource>();
            }
            else
            {
                result.body = std::make_unique<http::BufferSource>(testMsgContentType, testMsgBody);
            }

            completionHandler(std::move(result));
        }
        else
        {
            completionHandler(http::StatusCode::badRequest);
        }
    }

    std::optional<bool> m_securityExpectation;
};

//-------------------------------------------------------------------------------------------------

class Proxy:
    public BasicComponentTest
{
public:
    void SetUp() override
    {
        testHttpServer()->registerRequestProcessor(
            testPath,
            [this]()
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                return std::make_unique<VmsGatewayProxyTestHandler>(m_securityExpectation);
            });
    }

    void testProxyUrl(
        const QString& url,
        nx::network::http::StatusCode::Value expectedReponseStatusCode)
    {
        testProxyUrl(url,
            std::vector<nx::network::http::StatusCode::Value>{expectedReponseStatusCode});
    }

    void testProxyUrl(
        const QString& url,
        std::vector<nx::network::http::StatusCode::Value> expectedReponseStatusCodes
            = {nx::network::http::StatusCode::ok})
    {
        HttpClient httpClient{nx::network::ssl::kAcceptAnyCertificate};
        testProxyUrl(&httpClient, url, std::move(expectedReponseStatusCodes));
    }

    void testProxyUrl(
        nx::network::http::HttpClient* const httpClient,
        const QString& url,
        std::vector<nx::network::http::StatusCode::Value> expectedReponseStatusCodes)
    {
        NX_INFO(this, nx::format("testProxyUrl(%1)").arg(url));
        httpClient->setResponseReadTimeout(std::chrono::minutes(17));
        ASSERT_TRUE(httpClient->doGet(url));
        ASSERT_TRUE(
            std::find(
                expectedReponseStatusCodes.begin(),
                expectedReponseStatusCodes.end(),
                httpClient->response()->statusLine.statusCode) != expectedReponseStatusCodes.end())
            << "Actual: " << httpClient->response()->statusLine.statusCode;

        if (httpClient->response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
            return;

        ASSERT_EQ(
            testMsgContentType,
            nx::network::http::getHeaderValue(httpClient->response()->headers, "Content-Type"));

        nx::Buffer msgBody;
        while (!httpClient->eof())
            msgBody += httpClient->fetchMessageBodyBuffer();

        ASSERT_EQ(testMsgBody, msgBody);
    }

    void expectSecurity(std::optional<bool> isEnabled)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_securityExpectation = isEnabled;
    }

private:
    nx::Mutex m_mutex;
    std::optional<bool> m_securityExpectation;
};

//-------------------------------------------------------------------------------------------------

TEST_F(Proxy, IpSpecified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    // Default port
    testProxyUrl(
        nx::format("http://%1/%2%3").args(
            endpoint(),
            testHttpServer()->serverAddress().address,
            testPathAndQuery),
        nx::network::http::StatusCode::ok);

    // Specified
    testProxyUrl(
        nx::format("http://%1/%2%3").args(
            endpoint(),
            testHttpServer()->serverAddress(),
            testPathAndQuery),
        nx::network::http::StatusCode::ok);

    // Wrong path
    testProxyUrl(
        nx::format("http://%1/%2").args(
            endpoint(),
            testHttpServer()->serverAddress()),
        nx::network::http::StatusCode::notFound);
}

TEST_F(Proxy, failure_is_returned_when_unreachable_target_endpoint_is_specified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    testProxyUrl(
        nx::format("http://%1/%2:777%3").args(
            endpoint(),
            testHttpServer()->serverAddress().address,
            testPathAndQuery),
        {nx::network::http::StatusCode::serviceUnavailable,
            nx::network::http::StatusCode::internalServerError});
}

TEST_F(Proxy, SslEnabled)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/preferedSslMode", "followIncomingConnection");
    ASSERT_TRUE(startAndWaitUntilStarted());

    expectSecurity(false);
    testProxyUrl(nx::format("http://%1/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(true);
    testProxyUrl(nx::format("https://%1/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(false);
    testProxyUrl(nx::format("https://%1/http:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(false);
    testProxyUrl(nx::format("http://%1/http:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(true);
    testProxyUrl(nx::format("http://%1/ssl:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(true);
    testProxyUrl(nx::format("http://%1/https:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));
}

TEST_F(Proxy, SslEnforced)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/preferedSslMode", "enabled");
    ASSERT_TRUE(startAndWaitUntilStarted());

    expectSecurity(true);
    testProxyUrl(nx::format("http://%1/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(true);
    testProxyUrl(nx::format("https://%1/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(false);
    testProxyUrl(nx::format("http://%1/http:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));
}

TEST_F(Proxy, SslRestricted)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/preferedSslMode", "disabled");
    ASSERT_TRUE(startAndWaitUntilStarted());

    expectSecurity(false);
    testProxyUrl(nx::format("http://%1/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(false);
    testProxyUrl(nx::format("https://%1/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(false);
    testProxyUrl(nx::format("https://%1/http:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(false);
    testProxyUrl(nx::format("http://%1/http:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(true);
    testProxyUrl(nx::format("http://%1/ssl:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    expectSecurity(true);
    testProxyUrl(nx::format("http://%1/https:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));
}

TEST_F(Proxy, SslForbidden)
{
    addArg("-http/sslSupport", "false");
    addArg("-cloudConnect/preferedSslMode", "followIncomingConnection");
    ASSERT_TRUE(startAndWaitUntilStarted());
    expectSecurity(false);

    testProxyUrl(nx::format("http://%1/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    testProxyUrl(nx::format("http://%1/http:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));

    testProxyUrl(nx::format("http://%1/ssl:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery),
        nx::network::http::StatusCode::forbidden);

    testProxyUrl(nx::format("http://%1/https:%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery),
        nx::network::http::StatusCode::forbidden);
}

TEST_F(Proxy, IpForbidden)
{
    ASSERT_TRUE(startAndWaitUntilStarted(false, false, false));

    // Ip Address Is forbidden
    testProxyUrl(
        nx::format("http://%1/%2%3").args(
            endpoint(),
            testHttpServer()->serverAddress().address,
            testPathAndQuery),
        nx::network::http::StatusCode::forbidden);
}

//testing proxying in case of request line like "GET http://192.168.0.1:2343/some/path HTTP/1.1"
TEST_F(Proxy, proxyByRequestUrl)
{
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-cloudConnect/replaceHostAddressWithPublicAddress", "false");
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    const auto targetUrl = nx::format("http://%1%2").args(
        testHttpServer()->serverAddress(), testPathAndQuery);
    HttpClient httpClient{nx::network::ssl::kAcceptAnyCertificate};
    httpClient.setProxyVia(
        endpoint(), /*isSecure*/ false, nx::network::ssl::kAcceptAnyCertificate);
    testProxyUrl(&httpClient, targetUrl, {nx::network::http::StatusCode::ok});
}

TEST_F(Proxy, proxyingChunkedBody)
{
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-cloudConnect/replaceHostAddressWithPublicAddress", "false");
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    const auto targetUrl = nx::format("http://%1%2").args(
        testHttpServer()->serverAddress(), checkuedTestPathAndQuery);
    HttpClient httpClient{nx::network::ssl::kAcceptAnyCertificate};
    httpClient.setProxyVia(
        endpoint(), /*isSecure*/ false, nx::network::ssl::kAcceptAnyCertificate);
    testProxyUrl(&httpClient, targetUrl, {nx::network::http::StatusCode::ok});
}

TEST_F(Proxy, proxyingUndefinedContentLength)
{
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-cloudConnect/replaceHostAddressWithPublicAddress", "false");
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    const auto targetUrl = nx::format("http://%1%2?%3&undefinedContentLength").args(
        testHttpServer()->serverAddress(), testPath, testQuery);

    HttpClient httpClient{nx::network::ssl::kAcceptAnyCertificate};
    httpClient.setProxyVia(
        endpoint(), /*isSecure*/ false, nx::network::ssl::kAcceptAnyCertificate);
    testProxyUrl(&httpClient, targetUrl, {nx::network::http::StatusCode::ok});
}

TEST_F(Proxy, ModRewrite)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/sslAllowed", "true");
    ASSERT_TRUE(startAndWaitUntilStarted());

    testProxyUrl(nx::format("http://%1/gateway/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress().address,
        testPathAndQuery));

    testProxyUrl(nx::format("https://%1/gateway/%2%3").args(
        endpoint(),
        testHttpServer()->serverAddress(),
        testPathAndQuery));
}

//-------------------------------------------------------------------------------------------------

static constexpr char kEmptyResourcePath[] = "/proxy/empty";
static constexpr char kSaveRequestPath[] = "/proxy/save-request";

class ProxyNewTest:
    public Proxy
{
    using base_type = Proxy;

protected:
    void whenRequestEmptyResource()
    {
        fetchResource(kEmptyResourcePath);
    }

    void whenSendRequestSpecifyingSupportedAndUnsupportedEncodings()
    {
        fetchResource(
            kSaveRequestPath,
            {{"Accept-Encoding", "invalid-encoding, deflate, gzip"}});
    }

    void whenSendRequestSpecifyingOnlyUnsupportedEncodings()
    {
        fetchResource(
            kSaveRequestPath,
            {{"Accept-Encoding", "invalid-encoding"}});
    }

    void thenEmptyResponseIsDelivered()
    {
        ASSERT_EQ(StatusCode::noContent, m_response->statusLine.statusCode);
        ASSERT_TRUE(m_msgBody.empty());
    }

    void thenTheRequestIsDeliveredToTheTarget()
    {
        m_lastProxiedRequest = m_proxiedRequests.pop();
    }

    void thenRequestIsFailed()
    {
        ASSERT_NE(StatusCode::ok, m_response->statusLine.statusCode);
    }

    void andOnlySupportedEncodingArePresentInTheProxiedRequest()
    {
        header::AcceptEncodingHeader acceptEncoding(
            getHeaderValue(m_lastProxiedRequest.headers, "Accept-Encoding"));

        for (const auto& [encoding, qValue]: acceptEncoding.allEncodings())
        {
            if (qValue > 0.0)
            {
                ASSERT_TRUE(HttpStreamReader::isEncodingSupported(encoding));
            }
        }
    }

    void andNoHeaderPresent(const std::string_view& name)
    {
        ASSERT_EQ(0, m_lastProxiedRequest.headers.count(name));
    }

private:
    std::optional<Response> m_response;
    nx::Buffer m_msgBody;
    nx::utils::SyncQueue<Request> m_proxiedRequests;
    Request m_lastProxiedRequest;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        base_type::SetUp();

        testHttpServer()->registerRequestProcessorFunc(
            kEmptyResourcePath,
            std::bind(&ProxyNewTest::returnEmptyHttpResponse, this, _1, _2));

        testHttpServer()->registerRequestProcessorFunc(
            kSaveRequestPath,
            std::bind(&ProxyNewTest::saveRequest, this, _1, _2));

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void returnEmptyHttpResponse(
        RequestContext /*requestContext*/,
        RequestProcessedHandler completionHandler)
    {
        completionHandler(StatusCode::noContent);
    }

    void saveRequest(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler)
    {
        m_proxiedRequests.push(std::move(requestContext.request));
        completionHandler(StatusCode::ok);
    }

    void fetchResource(
        const char* path,
        std::vector<HttpHeader> headers = {})
    {
        const nx::utils::Url url(nx::format("http://%1/%2%3").args(
            endpoint(), testHttpServer()->serverAddress(), path));
        HttpClient httpClient{nx::network::ssl::kAcceptAnyCertificate};
        for (const auto& header: headers)
            httpClient.addAdditionalHeader(header.first, header.second);
        httpClient.setResponseReadTimeout(nx::network::kNoTimeout);
        ASSERT_TRUE(httpClient.doGet(url));
        ASSERT_NE(nullptr, httpClient.response());

        m_response = *httpClient.response();
        while (!httpClient.eof())
            m_msgBody += httpClient.fetchMessageBodyBuffer();
    }
};

TEST_F(ProxyNewTest, response_contains_no_content)
{
    whenRequestEmptyResource();
    thenEmptyResponseIsDelivered();
}

TEST_F(ProxyNewTest, proxy_server_does_not_forward_unsupported_encodings)
{
    whenSendRequestSpecifyingSupportedAndUnsupportedEncodings();

    thenTheRequestIsDeliveredToTheTarget();
    andOnlySupportedEncodingArePresentInTheProxiedRequest();
}

TEST_F(ProxyNewTest, accept_encoding_header_is_removed_by_proxy_if_no_supported_encodings_found)
{
    whenSendRequestSpecifyingOnlyUnsupportedEncodings();

    thenTheRequestIsDeliveredToTheTarget();
    andNoHeaderPresent("Accept-Encoding");
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
