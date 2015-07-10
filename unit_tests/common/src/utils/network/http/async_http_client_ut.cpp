#include <common/common_globals.h>
#include <utils/network/http/asynchttpclient.h>

#include <gtest.h>
#include <QThread>

namespace nx_http {

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

} // namespace nx_http
