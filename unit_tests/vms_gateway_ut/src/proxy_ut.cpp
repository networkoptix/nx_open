#include "test_setup.h"

#include <nx/network/http/httpclient.h>


namespace nx {
namespace cloud {
namespace gateway {
namespace test {

const nx_http::BufferType testPath("/test");
const nx_http::BufferType testMsgBody("bla-bla-bla");
const nx_http::BufferType contentType("text/plain");

class VmsGatewayProxyTest
:
    public VmsGatewayFunctionalTest
{
public:
    void SetUp() override
    {
        testHttpServer()->registerStaticProcessor(
            testPath,
            testMsgBody,
            contentType);
    }

    void testProxyUrl(
        const QUrl& url,
        nx_http::StatusCode::Value status = nx_http::StatusCode::ok)
    {
        nx_http::HttpClient httpClient;
        ASSERT_TRUE(httpClient.doGet(url));
        ASSERT_EQ(status, httpClient.response()->statusLine.statusCode);

        if (status != nx_http::StatusCode::ok)
            return;

        ASSERT_EQ(
            contentType,
            nx_http::getHeaderValue(httpClient.response()->headers, "Content-Type"));

        nx_http::BufferType msgBody;
        while (!httpClient.eof())
            msgBody += httpClient.fetchMessageBodyBuffer();

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
        .arg(QLatin1String(testPath))));

    // Specified
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(QLatin1String(testPath))));

    // Wrong port
    testProxyUrl(QUrl(lit("http://%1/%2:777%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(QLatin1String(testPath))),
        nx_http::StatusCode::serviceUnavailable);

    // Wrong path
    testProxyUrl(QUrl(lit("http://%1/%2")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())),
        nx_http::StatusCode::notFound);
}

TEST_F(VmsGatewayProxyTest, NoDefaultPort)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, false));

    // Default port is not here
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(QLatin1String(testPath))),
        nx_http::StatusCode::serviceUnavailable);

    // Port specified
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().toString())
        .arg(QLatin1String(testPath))));
}

TEST_F(VmsGatewayProxyTest, IpForbidden)
{
    ASSERT_TRUE(startAndWaitUntilStarted(false, false, false));

    // Ip Address Is forbidden
    testProxyUrl(QUrl(lit("http://%1/%2%3")
        .arg(endpoint().toString())
        .arg(testHttpServer()->serverAddress().address.toString())
        .arg(QLatin1String(testPath))),
        nx_http::StatusCode::forbidden);
}

}   // namespace test
}   // namespace gateway
}   // namespace cloud
}   // namespace nx
