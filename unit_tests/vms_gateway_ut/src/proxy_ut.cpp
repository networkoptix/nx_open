#include "test_setup.h"

#include <QtCore/QUrlQuery>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/chunked_transfer_encoder.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

static const QString testPath("/test");
static const QString testQuery("testQuery");
static const QString testPathAndQuery(lit("%1?%2").arg(testPath).arg(testQuery));
static const QString checkuedTestPathAndQuery(lit("%1?%2&chunked").arg(testPath).arg(testQuery));
static const nx::network::http::BufferType testMsgBody("bla-bla-bla");
static const nx::network::http::BufferType testMsgContentType("text/plain");

class UndefinedContentLengthBufferSource: public nx::network::http::BufferSource
{
public:
    UndefinedContentLengthBufferSource(): nx::network::http::BufferSource(testMsgContentType, testMsgBody) {}
    virtual boost::optional<uint64_t> contentLength() const override { return boost::none; }
};

class VmsGatewayProxyTestHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    VmsGatewayProxyTestHandler(boost::optional<bool> securityExpectation):
        m_securityExpectation(securityExpectation)
    {
    }

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler )
    {
        if (m_securityExpectation)
        {
            EXPECT_EQ(m_securityExpectation.get(), connection->isSsl());
        }

        QUrlQuery requestQuery(request.requestLine.url.query());

        if (request.requestLine.url.path() == testPath &&
            requestQuery.hasQueryItem(testQuery))
        {
            std::unique_ptr<nx::network::http::AbstractMsgBodySource> bodySource;
            if (requestQuery.hasQueryItem("chunked"))
            {
                response->headers.emplace("Transfer-Encoding", "chunked");
                bodySource = std::make_unique<nx::network::http::BufferSource>(testMsgContentType,
                    nx::network::http::QnChunkedTransferEncoder::serializeSingleChunk(testMsgBody)+"0\r\n\r\n");
            }
            else if (requestQuery.hasQueryItem("undefinedContentLength"))
            {
                bodySource = std::make_unique<UndefinedContentLengthBufferSource>();
            }
            else
            {
                bodySource = std::make_unique<nx::network::http::BufferSource>(testMsgContentType, testMsgBody);
            }

            completionHandler(nx::network::http::RequestResult(
                nx::network::http::StatusCode::ok, std::move(bodySource)));
        }
        else
        {
            completionHandler(nx::network::http::StatusCode::badRequest);
        }
    }

    boost::optional<bool> m_securityExpectation;
};

//-------------------------------------------------------------------------------------------------

class Proxy:
    public BasicComponentTest
{
public:
    void SetUp() override
    {
        testHttpServer()->registerRequestProcessor<VmsGatewayProxyTestHandler>(
            testPath,
            [this]()
            {
                QnMutexLocker lock(&m_mutex);
                return std::make_unique<VmsGatewayProxyTestHandler>(m_securityExpectation);
            });
    }

    void testProxyUrl(
        const nx::utils::Url& url,
        nx::network::http::StatusCode::Value expectedReponseStatusCode)
    {
        testProxyUrl(url, {{expectedReponseStatusCode}});
    }

    void testProxyUrl(
        const nx::utils::Url& url,
        std::vector<nx::network::http::StatusCode::Value> expectedReponseStatusCodes
            = {nx::network::http::StatusCode::ok})
    {
        nx::network::http::HttpClient httpClient;
        testProxyUrl(&httpClient, url, std::move(expectedReponseStatusCodes));
    }

    void testProxyUrl(
        nx::network::http::HttpClient* const httpClient,
        const nx::utils::Url& url,
        std::vector<nx::network::http::StatusCode::Value> expectedReponseStatusCodes)
    {
        NX_LOGX(lm("testProxyUrl(%1)").arg(url), cl_logINFO);
        httpClient->setResponseReadTimeoutMs(1000*1000);
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

        nx::network::http::BufferType msgBody;
        while (!httpClient->eof())
            msgBody += httpClient->fetchMessageBodyBuffer();

        ASSERT_EQ(testMsgBody, msgBody);
    }

    void expectSecurity(boost::optional<bool> isEnabled)
    {
        QnMutexLocker lock(&m_mutex);
        m_securityExpectation = isEnabled;
    }

private:
    QnMutex m_mutex;
    boost::optional<bool> m_securityExpectation{boost::none};
};

//-------------------------------------------------------------------------------------------------

TEST_F(Proxy, IpSpecified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    // Default port
    testProxyUrl(
        nx::utils::Url(lit("http://%1/%2%3")
            .arg(endpoint().toString())
            .arg(testHttpServer()->serverAddress().address.toString())
            .arg(testPathAndQuery)),
        {nx::network::http::StatusCode::ok});

    // Specified
    testProxyUrl(
        nx::utils::Url(lit("http://%1/%2%3")
            .arg(endpoint().toString())
            .arg(testHttpServer()->serverAddress().toString())
            .arg(testPathAndQuery)),
        {nx::network::http::StatusCode::ok});

    // Wrong path
    testProxyUrl(
        nx::utils::Url(lit("http://%1/%2")
            .arg(endpoint().toString())
            .arg(testHttpServer()->serverAddress().toString())),
        {nx::network::http::StatusCode::notFound});
}

TEST_F(Proxy, failure_is_returned_when_unreachable_target_endpoint_is_specified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    testProxyUrl(
        nx::utils::Url(lit("http://%1/%2:777%3")
            .arg(endpoint().toString())
            .arg(testHttpServer()->serverAddress().address.toString())
            .arg(testPathAndQuery)),
        {nx::network::http::StatusCode::serviceUnavailable, nx::network::http::StatusCode::internalServerError});
}

TEST_F(Proxy, SslEnabled)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/preferedSslMode", "followIncomingConnection");
    ASSERT_TRUE(startAndWaitUntilStarted());

    expectSecurity(false);
    testProxyUrl(nx::utils::Url(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(nx::utils::Url(lit("https://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(nx::utils::Url(lit("https://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(nx::utils::Url(lit("http://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(nx::utils::Url(lit("http://%1/ssl:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(nx::utils::Url(lit("http://%1/https:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));
}

TEST_F(Proxy, SslEnforced)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/preferedSslMode", "enabled");
    ASSERT_TRUE(startAndWaitUntilStarted());

    expectSecurity(true);
    testProxyUrl(nx::utils::Url(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(nx::utils::Url(lit("https://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(nx::utils::Url(lit("http://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));
}

TEST_F(Proxy, SslRestricted)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/preferedSslMode", "disabled");
    ASSERT_TRUE(startAndWaitUntilStarted());

    expectSecurity(false);
    testProxyUrl(nx::utils::Url(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(nx::utils::Url(lit("https://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(nx::utils::Url(lit("https://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(nx::utils::Url(lit("http://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(nx::utils::Url(lit("http://%1/ssl:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(nx::utils::Url(lit("http://%1/https:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));
}

TEST_F(Proxy, SslForbidden)
{
    addArg("-http/sslSupport", "false");
    addArg("-cloudConnect/preferedSslMode", "followIncomingConnection");
    ASSERT_TRUE(startAndWaitUntilStarted());
    expectSecurity(false);

    testProxyUrl(nx::utils::Url(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    testProxyUrl(nx::utils::Url(lit("http://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    testProxyUrl(nx::utils::Url(lit("http://%1/ssl:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)),
        nx::network::http::StatusCode::forbidden);

    testProxyUrl(nx::utils::Url(lit("http://%1/https:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)),
        nx::network::http::StatusCode::forbidden);
}

TEST_F(Proxy, IpForbidden)
{
    ASSERT_TRUE(startAndWaitUntilStarted(false, false, false));

    // Ip Address Is forbidden
    testProxyUrl(nx::utils::Url(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(testPathAndQuery)),
        nx::network::http::StatusCode::forbidden);
}

//testing proxying in case of request line like "GET http://192.168.0.1:2343/some/path HTTP/1.1"
TEST_F(Proxy, proxyByRequestUrl)
{
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-cloudConnect/replaceHostAddressWithPublicAddress", "false");
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    const nx::utils::Url targetUrl =
        lit("http://%1%2").arg(testHttpServer()->serverAddress().toString())
            .arg(testPathAndQuery);
    nx::network::http::HttpClient httpClient;
    httpClient.setProxyVia(endpoint());
    testProxyUrl(&httpClient, targetUrl, {nx::network::http::StatusCode::ok});
}

TEST_F(Proxy, proxyingChunkedBody)
{
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-cloudConnect/replaceHostAddressWithPublicAddress", "false");
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    const nx::utils::Url targetUrl =
       lit("http://%1%2").arg(testHttpServer()->serverAddress().toString())
            .arg(checkuedTestPathAndQuery);
    nx::network::http::HttpClient httpClient;
    httpClient.setProxyVia(endpoint());
    testProxyUrl(&httpClient, targetUrl, {nx::network::http::StatusCode::ok});
}

TEST_F(Proxy, proxyingUndefinedContentLength)
{
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-cloudConnect/replaceHostAddressWithPublicAddress", "false");
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    const nx::utils::Url targetUrl =
        lit("http://%1%2").arg(testHttpServer()->serverAddress().toString())
        .arg(lit("%1?%2&undefinedContentLength").arg(testPath).arg(testQuery));

    nx::network::http::HttpClient httpClient;
    httpClient.setProxyVia(endpoint());
    testProxyUrl(&httpClient, targetUrl, {nx::network::http::StatusCode::ok});
}

TEST_F(Proxy, ModRewrite)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/sslAllowed", "true");
    ASSERT_TRUE(startAndWaitUntilStarted());

    testProxyUrl(nx::utils::Url(lit("http://%1/gateway/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(testPathAndQuery)));

    testProxyUrl(nx::utils::Url(lit("https://%1/gateway/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));
}

//-------------------------------------------------------------------------------------------------

static const char* kEmptyResourcePath = "/proxy/empty";

class ProxyNewTest:
    public Proxy
{
    using base_type = Proxy;

protected:
    void whenRequestEmptyResource()
    {
        fetchResource(kEmptyResourcePath);
    }

    void thenEmptyResponseIsDelivered()
    {
        ASSERT_EQ(nx::network::http::StatusCode::noContent, m_response->statusLine.statusCode);
        ASSERT_TRUE(m_msgBody.isEmpty());
    }

private:
    boost::optional<nx::network::http::Response> m_response;
    nx::Buffer m_msgBody;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        base_type::SetUp();

        testHttpServer()->registerRequestProcessorFunc(
            kEmptyResourcePath,
            std::bind(&ProxyNewTest::returnEmptyHttpResponse, this, _1, _2, _3, _4, _5));

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void returnEmptyHttpResponse(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        completionHandler(nx::network::http::StatusCode::noContent);
    }

    void fetchResource(const char* path)
    {
        const nx::utils::Url url(lm("http://%1/%2%3").arg(endpoint()).arg(testHttpServer()->serverAddress()).arg(path));
        nx::network::http::HttpClient httpClient;
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

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
