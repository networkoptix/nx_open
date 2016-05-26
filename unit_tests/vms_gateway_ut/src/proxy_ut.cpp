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

TEST_F(VmsGatewayFunctionalTest, simple)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    const nx_http::BufferType testPath("/test");
    const nx_http::BufferType testMsgBody("bla-bla-bla");
    const nx_http::BufferType contentType("text/plain");

    testHttpServer()->registerStaticProcessor(
        testPath,
        testMsgBody,
        contentType);

    //testing proxy
    nx_http::HttpClient httpClient;
    const QUrl proxyUrl(lit("http://%1/127.0.0.1%2").arg(endpoint().toString()).arg(QLatin1String(testPath)));
    
    ASSERT_TRUE(httpClient.doGet(proxyUrl));
    ASSERT_EQ(
        nx_http::StatusCode::ok,
        httpClient.response()->statusLine.statusCode);
    ASSERT_EQ(
        contentType,
        nx_http::getHeaderValue(httpClient.response()->headers, "Content-Type"));

    nx_http::BufferType msgBody;
    while (!httpClient.eof())
        msgBody += httpClient.fetchMessageBodyBuffer();
    ASSERT_EQ(
        testMsgBody,
        msgBody);
}

}   // namespace test
}   // namespace gateway
}   // namespace cloud
}   // namespace nx
