/**********************************************************
* May 19, 2016
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/network/http/httpclient.h>
#include <libconnection_mediator/src/test_support/mediator_functional_test.h>

#include "test_setup.h"


namespace nx {
namespace cloud {
namespace gateway {
namespace test {

const nx_http::BufferType testPath("/test");
const nx_http::BufferType testMsgBody("bla-bla-bla");
const nx_http::BufferType contentType("text/plain");

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

TEST_F(VmsGatewayFunctionalTest, IpSpecified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, true));
    testHttpServer()->registerStaticProcessor(
        testPath,
        testMsgBody,
        contentType);

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

TEST_F(VmsGatewayFunctionalTest, IpSpecified_noDefaultPort)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false));
    testHttpServer()->registerStaticProcessor(
        testPath,
        testMsgBody,
        contentType);

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

TEST_F(VmsGatewayFunctionalTest, IpForbidden)
{
    ASSERT_TRUE(startAndWaitUntilStarted(false, false));
    testHttpServer()->registerStaticProcessor(
        testPath,
        testMsgBody,
        contentType);

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
