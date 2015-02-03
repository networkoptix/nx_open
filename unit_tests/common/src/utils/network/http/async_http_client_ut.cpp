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

#if 0
TEST( AsyncHttpClient, KeepAlive )
{
    //TODO #ak use local http server

    static const QUrl url( "http://192.168.0.1/girls/candice_swanepoel_07_original.jpg" );
    static const int TEST_RUNS = 2;

    nx_http::HttpClient client;

    nx::Buffer msgBody;
    for( int i = 0; i < TEST_RUNS; ++i )
    {
        ASSERT_TRUE( client.doGet( url ) );
        ASSERT_TRUE( client.response() );
        ASSERT_EQ( client.response()->statusLine.statusCode, nx_http::StatusCode::ok );
        nx::Buffer newMsgBody;
        while( !client.eof() )
            newMsgBody += client.fetchMessageBodyBuffer();
        if( i == 0 )
            msgBody = newMsgBody;
        else
            ASSERT_EQ( msgBody, newMsgBody );
    }
}

TEST( AsyncHttpClient, KeepAlive2 )
{
    QUrl url( "http://192.168.0.1:7001/ec2/testConnection" );
    url.setUserName( "admin" );
    url.setPassword( "123" );
    static const int TEST_RUNS = 2;

    nx::Buffer msgBody;
    for( int i = 0; i < TEST_RUNS; ++i )
    {
        nx_http::HttpClient client;
        ASSERT_TRUE( client.doGet( url ) );
        ASSERT_TRUE( client.response() );
        ASSERT_EQ( nx_http::StatusCode::ok, client.response()->statusLine.statusCode );
        nx::Buffer newMsgBody;
        while( !client.eof() )
            newMsgBody += client.fetchMessageBodyBuffer();
        if( i == 0 )
            msgBody = newMsgBody;
        else
            ASSERT_EQ( msgBody, newMsgBody );
    }
}

TEST( AsyncHttpClient, KeepAlive3 )
{
    QUrl url( "http://192.168.0.194:7001/ec2/events?guid=%7Be7209f3e-9ebe-6ebb-3e99-e5acd61c228c%7D&runtime-guid=%7B83862a97-b7b8-4dbc-bb8f-64847f23e6d5%7D&system-identity-time=0" );
    url.setUserName( "admin" );
    url.setPassword( "123" );
    static const int TEST_RUNS = 2;

    nx::Buffer msgBody;
    for( int i = 0; i < TEST_RUNS; ++i )
    {
        nx_http::HttpClient client;
        client.addRequestHeader( "NX-EC-SYSTEM-NAME", "ak_ec_2.3" );
        ASSERT_TRUE( client.doGet( url ) );
        ASSERT_TRUE( client.response() );
        ASSERT_EQ( nx_http::StatusCode::ok, client.response()->statusLine.statusCode );
        nx::Buffer newMsgBody;
        while( !client.eof() )
            newMsgBody += client.fetchMessageBodyBuffer();
        if( i == 0 )
            msgBody = newMsgBody;
        else
            ASSERT_EQ( msgBody, newMsgBody );
    }
}
#endif

/*!
    This test verifies that AbstractCommunicatingSocket::cancelAsyncIO method works fine
*/
TEST_F( AsyncHttpClientTest, KeepAliveConnection )
{
}
