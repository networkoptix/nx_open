
#include "test_setup.h"

#include <QtCore/QUrlQuery>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/chunked_transfer_encoder.h>
#include <nx/network/http/httpclient.h>


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

class VmsGatewayProxyTestHandler
    :
    public nx_http::AbstractHttpRequestHandler
{
public:
    VmsGatewayProxyTestHandler()
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::HttpRequestProcessedHandler completionHandler )
    {
        QUrlQuery requestQuery(request.requestLine.url.query());

        if (request.requestLine.url.path() == testPath &&
            requestQuery.hasQueryItem(testQuery))
        {
            if (requestQuery.hasQueryItem("chunked"))
            {
                response->headers.emplace("Transfer-Encoding", "chunked");
                completionHandler(
                    nx_http::StatusCode::ok,
                    std::make_unique< nx_http::BufferSource >(
                        testMsgContentType,
                        nx_http::QnChunkedTransferEncoder::serializeSingleChunk(testMsgBody)+"0\r\n\r\n"),
                    nx_http::ConnectionEvents());
            }
            else
            {
                completionHandler(
                    nx_http::StatusCode::ok,
                    std::make_unique< nx_http::BufferSource >(testMsgContentType, testMsgBody),
                    nx_http::ConnectionEvents());
            }
        }
        else
        {
            completionHandler(
                nx_http::StatusCode::badRequest,
                nullptr,
                nx_http::ConnectionEvents());
        }
    }
};

class VmsGatewayProxyTest
:
    public VmsGatewayFunctionalTest
{
public:
    void SetUp() override
    {
        testHttpServer()->registerRequestProcessor<VmsGatewayProxyTestHandler>(testPath);
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
};

TEST_F(VmsGatewayProxyTest, IpSpecified)
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

TEST_F(VmsGatewayProxyTest, SslEnabled)
{
    addArg("-http/sslSupport", "true");
    ASSERT_TRUE(startAndWaitUntilStarted());

    testProxyUrl(QUrl(lit("http://%1/http:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    testProxyUrl(QUrl(lit("http://%1/ssl:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));

    testProxyUrl(QUrl(lit("http://%1/https:%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(testPathAndQuery)));
}

TEST_F(VmsGatewayProxyTest, SslForbidden)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

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

TEST_F(VmsGatewayProxyTest, IpForbidden)
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
TEST_F(VmsGatewayProxyTest, proxyByRequestUrl)
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

TEST_F(VmsGatewayProxyTest, proxyingChunkedBody)
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

}   // namespace test
}   // namespace gateway
}   // namespace cloud
}   // namespace nx
