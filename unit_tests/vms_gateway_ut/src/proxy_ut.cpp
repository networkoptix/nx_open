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
static const nx_http::BufferType testMsgBody("bla-bla-bla");
static const nx_http::BufferType testMsgContentType("text/plain");

class VmsGatewayProxyTestHandler:
    public nx_http::AbstractHttpRequestHandler
{
public:
    VmsGatewayProxyTestHandler(boost::optional<bool> securityExpectation):
        m_securityExpectation(securityExpectation)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler )
    {
        if (m_securityExpectation)
            EXPECT_EQ(m_securityExpectation.get(), connection->isSsl());

        QUrlQuery requestQuery(request.requestLine.url.query());

        if (request.requestLine.url.path() == testPath &&
            requestQuery.hasQueryItem(testQuery))
        {
            if (requestQuery.hasQueryItem("chunked"))
            {
                response->headers.emplace("Transfer-Encoding", "chunked");
                completionHandler(
                    nx_http::RequestResult(
                        nx_http::StatusCode::ok,
                        std::make_unique< nx_http::BufferSource >(
                            testMsgContentType,
                            nx_http::QnChunkedTransferEncoder::serializeSingleChunk(testMsgBody)+"0\r\n\r\n")));
            }
            else
            {
                completionHandler(
                    nx_http::RequestResult(
                        nx_http::StatusCode::ok,
                        std::make_unique< nx_http::BufferSource >(testMsgContentType, testMsgBody)));
            }
        }
        else
        {
            completionHandler(nx_http::StatusCode::badRequest);
        }
    }

    boost::optional<bool> m_securityExpectation;
};

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
        const QUrl& url,
        nx_http::StatusCode::Value expectedReponseStatusCode = nx_http::StatusCode::ok)
    {
        nx_http::HttpClient httpClient;
        testProxyUrl(&httpClient, url, expectedReponseStatusCode);
    }

    void testProxyUrl(
        nx_http::HttpClient* const httpClient,
        const QUrl& url,
        nx_http::StatusCode::Value expectedReponseStatusCode)
    {
        NX_LOGX(lm("testProxyUrl(%1)").arg(url), cl_logINFO);
        httpClient->setResponseReadTimeoutMs(1000*1000);
        ASSERT_TRUE(httpClient->doGet(url));
        ASSERT_EQ(
            expectedReponseStatusCode,
            httpClient->response()->statusLine.statusCode);

        if (expectedReponseStatusCode != nx_http::StatusCode::ok)
            return;

        ASSERT_EQ(
            testMsgContentType,
            nx_http::getHeaderValue(httpClient->response()->headers, "Content-Type"));

        nx_http::BufferType msgBody;
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

TEST_F(Proxy, IpSpecified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    // Default port
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(testPathAndQuery)));

    // Specified
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    // Wrong port
    testProxyUrl(QUrl(lit("http://%1/%2:777%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(testPathAndQuery)),
        nx_http::StatusCode::serviceUnavailable);

    // Wrong path
    testProxyUrl(QUrl(lit("http://%1/%2")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())),
        nx_http::StatusCode::notFound);
}

TEST_F(Proxy, SslEnabled)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/preferedSslMode", "followIncomingConnection");
    ASSERT_TRUE(startAndWaitUntilStarted());

    expectSecurity(false);
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(QUrl(lit("https://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(QUrl(lit("https://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(QUrl(lit("http://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(QUrl(lit("http://%1/ssl:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(QUrl(lit("http://%1/https:%2%3")
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
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(QUrl(lit("https://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(QUrl(lit("http://%1/http:%2%3")
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
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(QUrl(lit("https://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(QUrl(lit("https://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(false);
    testProxyUrl(QUrl(lit("http://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(QUrl(lit("http://%1/ssl:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    expectSecurity(true);
    testProxyUrl(QUrl(lit("http://%1/https:%2%3")
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

    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    testProxyUrl(QUrl(lit("http://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    testProxyUrl(QUrl(lit("http://%1/ssl:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)),
        nx_http::StatusCode::forbidden);

    testProxyUrl(QUrl(lit("http://%1/https:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)),
        nx_http::StatusCode::forbidden);
}

TEST_F(Proxy, IpForbidden)
{
    ASSERT_TRUE(startAndWaitUntilStarted(false, false, false));

    // Ip Address Is forbidden
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(testPathAndQuery)),
        nx_http::StatusCode::forbidden);
}

//testing proxying in case of request line like "GET http://192.168.0.1:2343/some/path HTTP/1.1"
TEST_F(Proxy, proxyByRequestUrl)
{
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-cloudConnect/replaceHostAddressWithPublicAddress", "false");
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    const QUrl targetUrl =
        lit("http://%1%2").arg(testHttpServer()->serverAddress().toString())
            .arg(testPathAndQuery);
    nx_http::HttpClient httpClient;
    httpClient.setProxyVia(endpoint());
    testProxyUrl(&httpClient, targetUrl, nx_http::StatusCode::ok);
}

TEST_F(Proxy, proxyingChunkedBody)
{
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-cloudConnect/replaceHostAddressWithPublicAddress", "false");
    ASSERT_TRUE(startAndWaitUntilStarted(true, true, false));

    const QUrl targetUrl =
       lit("http://%1%2").arg(testHttpServer()->serverAddress().toString())
            .arg(checkuedTestPathAndQuery);
    nx_http::HttpClient httpClient;
    httpClient.setProxyVia(endpoint());
    testProxyUrl(&httpClient, targetUrl, nx_http::StatusCode::ok);
}

TEST_F(Proxy, ModRewrite)
{
    addArg("-http/sslSupport", "true");
    addArg("-cloudConnect/sslAllowed", "true");
    ASSERT_TRUE(startAndWaitUntilStarted());

    testProxyUrl(QUrl(lit("http://%1/gateway/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(testPathAndQuery)));

    testProxyUrl(QUrl(lit("https://%1/gateway/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
