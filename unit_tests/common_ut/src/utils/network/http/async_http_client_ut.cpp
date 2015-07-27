/**********************************************************
* 26 dec 2014
* a.kolesnikov
***********************************************************/

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/http/httpclient.h>
#include <utils/network/http/server/http_stream_socket_server.h>

#include "test_http_server.h"

#include <QThread>

namespace nx_http {

class AsyncHttpClientTest
:
    public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
        testHttpServer.reset( new TestHttpServer() );
    }

    static void TearDownTestCase()
    {
        testHttpServer.reset();
    }

private:
    static std::unique_ptr<TestHttpServer> testHttpServer;
};

std::unique_ptr<TestHttpServer> AsyncHttpClientTest::testHttpServer;

//TODO #ak introduce built-in http server to automate AsyncHttpClient tests

void testHttpClientForFastRemove( const QUrl& url )
{
    EXPECT_TRUE( nx_http::AsyncHttpClient::create()->doGet( url ) );

    // use different delays (10us - 0.5s) to catch problems on different stages
    for( uint time = 10; time < 500000; time *= 2 )
    {
        const auto client = nx_http::AsyncHttpClient::create();
        EXPECT_TRUE( client->doGet( url ) );

        // kill the client after some delay
        QThread::usleep( time );
    }
}

TEST( AsyncHttpClient, FastRemove )
{
    testHttpClientForFastRemove( lit( "http://127.0.0.1/" ) );
    testHttpClientForFastRemove( lit( "http://localhost/" ) );
    testHttpClientForFastRemove( lit( "http://doestNotExist.host/" ) );
}

TEST( AsyncHttpClient, FastRemoveBadHost )
{
    QUrl url( lit( "http://doestNotExist.host/" ) );

    for( int i = 0; i < 1000; ++i )
    {
        const auto client = nx_http::AsyncHttpClient::create();
        EXPECT_TRUE( client->doGet( url ) );
        QThread::usleep( 100 );
    }
}

} // namespace nx_http
