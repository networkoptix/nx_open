/**********************************************************
* 26 dec 2014
* a.kolesnikov
***********************************************************/

#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/common/cpp14.h>
#include <utils/media/custom_output_stream.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/http/httpclient.h>
#include <utils/network/http/multipart_content_parser.h>
#include <utils/network/http/server/http_stream_socket_server.h>

#include "repeating_buffer_sender.h"
#include "test_http_server.h"


namespace nx_http {

class AsyncHttpClientTest
:
    public ::testing::Test
{
public:
    AsyncHttpClientTest()
    :
        m_testHttpServer( std::make_unique<TestHttpServer>() )
    {
    }

protected:
    std::unique_ptr<TestHttpServer> m_testHttpServer;
};

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
        std::this_thread::sleep_for( std::chrono::microseconds( time ) );
    }
}

TEST_F( AsyncHttpClientTest, FastRemove )
{
    testHttpClientForFastRemove( lit( "http://127.0.0.1/" ) );
    testHttpClientForFastRemove( lit( "http://localhost/" ) );
    testHttpClientForFastRemove( lit( "http://doestNotExist.host/" ) );
}

TEST_F( AsyncHttpClientTest, FastRemoveBadHost )
{
    QUrl url( lit( "http://doestNotExist.host/" ) );

    for( int i = 0; i < 1000; ++i )
    {
        const auto client = nx_http::AsyncHttpClient::create();
        EXPECT_TRUE( client->doGet( url ) );
        std::this_thread::sleep_for( std::chrono::microseconds( 100 ) );
    }
}

TEST_F( AsyncHttpClientTest, motionJpegRetrieval )
{
    static const int CONCURRENT_CLIENT_COUNT = 10;

    const nx::Buffer frame1 =
        "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxx\r\nxxxxxxxxxxxxxxxx2";

    const nx::Buffer testFrame = 
        "\r\n--fbdr"
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame1;

    m_testHttpServer->registerRequestProcessor<RepeatingBufferSender>(
        "/mjpg",
        [&testFrame]()->std::unique_ptr<RepeatingBufferSender>{
            return std::make_unique<RepeatingBufferSender>(
                "multipart/x-mixed-replace;boundary=--fbdr",
                testFrame );
        } );

    //fetching mjpeg with async http client
    const QUrl url( lit("http://127.0.0.1:%1/mjpg").arg( m_testHttpServer->serverAddress().port) );

    struct ClientContext
    {
        nx_http::AsyncHttpClientPtr client;
        nx_http::MultipartContentParser multipartParser;
    };

    auto checkReceivedContentFunc = [&]( const QnByteArrayConstRef& data ){
        ASSERT_EQ( data, frame1 );
    };

    std::vector<ClientContext> clients( CONCURRENT_CLIENT_COUNT );
    for( ClientContext& clientCtx: clients )
    {
        clientCtx.client = nx_http::AsyncHttpClient::create();
        clientCtx.multipartParser.setNextFilter( makeCustomOutputStream( checkReceivedContentFunc ) );
        QObject::connect(
            clientCtx.client.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
            clientCtx.client.get(),
            [&]( nx_http::AsyncHttpClientPtr client ) {
                clientCtx.multipartParser.processData( client->fetchMessageBodyBuffer() );
            },
            Qt::DirectConnection );
        if( !clientCtx.client->doGet( url ) )
        {
        }
    }

    std::this_thread::sleep_for( std::chrono::seconds( 20 ) );
}

} // namespace nx_http
