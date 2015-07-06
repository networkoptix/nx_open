#include <common/common_globals.h>
#include <utils/network/http/asynchttpclient.h>

#include <gtest.h>
#include <QThread>

namespace nx_http {

void testHttpClientForFastRemove( const QUrl& url )
{
    EXPECT_TRUE( std::make_shared<AsyncHttpClient>()->doGet( url ) );

    for( uint time = 10; time < 1000000; time *= 2 )
    {
        const auto client = std::make_shared<AsyncHttpClient>();
        EXPECT_TRUE( client->doGet( url ) );

        // Kill the client after some delay
        QThread::usleep( time );
    }
}

TEST( AsyncHttpClient, FastRemove )
{
    testHttpClientForFastRemove( lit( "localhost" ) );
    testHttpClientForFastRemove( lit( "doesNotExist" ) );
}

} // namespace nx_http
