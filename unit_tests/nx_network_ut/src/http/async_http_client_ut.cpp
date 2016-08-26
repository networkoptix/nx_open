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
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <utils/media/custom_output_stream.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/test_http_server.h>

#include "repeating_buffer_sender.h"


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

    TestHttpServer* testHttpServer()
    {
        return m_testHttpServer.get();
    }

protected:
    std::unique_ptr<TestHttpServer> m_testHttpServer;
};

//TODO #ak introduce built-in http server to automate AsyncHttpClient tests

void testHttpClientForFastRemove( const QUrl& url )
{
    nx_http::AsyncHttpClient::create()->doGet( url );

    // use different delays (10us - 0.5s) to catch problems on different stages
    for( uint time = 10; time < 500000; time *= 2 )
    {
        const auto client = nx_http::AsyncHttpClient::create();
        client->doGet( url );

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
        client->doGet( url );
        std::this_thread::sleep_for( std::chrono::microseconds( 100 ) );
    }
}

TEST_F( AsyncHttpClientTest, motionJpegRetrieval )
{
    static const int CONCURRENT_CLIENT_COUNT = 100;

    const nx::Buffer frame1 =
        "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxx\r\nxxxxxxxxxxxxxxxx2";
    const nx::String boundary = "fbdr";

    const nx::Buffer testFrame =
        "\r\n--"+boundary+
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        +frame1;

    m_testHttpServer->registerRequestProcessor<RepeatingBufferSender>(
        "/mjpg",
        [&]()->std::unique_ptr<RepeatingBufferSender>{
            return std::make_unique<RepeatingBufferSender>(
                "multipart/x-mixed-replace;boundary="+boundary,
                testFrame );
        } );

    ASSERT_TRUE( m_testHttpServer->bindAndListen() );

    //fetching mjpeg with async http client
    const QUrl url( lit("http://127.0.0.1:%1/mjpg").arg( m_testHttpServer->serverAddress().port) );

    struct ClientContext
    {
        ~ClientContext()
        {
            client->terminate();
            client.reset(); //ensuring client removed before multipartParser
        }

        nx_http::AsyncHttpClientPtr client;
        nx_http::MultipartContentParser multipartParser;
    };

    std::atomic<size_t> bytesProcessed( 0 );

    auto checkReceivedContentFunc = [&]( const QnByteArrayConstRef& data ){
        ASSERT_EQ( data, frame1 );
        bytesProcessed += data.size();
    };

    std::vector<ClientContext> clients( CONCURRENT_CLIENT_COUNT );
    for( ClientContext& clientCtx: clients )
    {
        clientCtx.client = nx_http::AsyncHttpClient::create();
        clientCtx.multipartParser.setNextFilter( makeCustomOutputStream( checkReceivedContentFunc ) );
        QObject::connect(
            clientCtx.client.get(), &nx_http::AsyncHttpClient::responseReceived,
            clientCtx.client.get(),
            [&]( nx_http::AsyncHttpClientPtr client ) {
                ASSERT_TRUE( client->response() != nullptr );
                ASSERT_EQ( client->response()->statusLine.statusCode, nx_http::StatusCode::ok );
                auto contentTypeIter = client->response()->headers.find( "Content-Type" );
                ASSERT_TRUE( contentTypeIter != client->response()->headers.end() );
                clientCtx.multipartParser.setContentType( contentTypeIter->second );
            },
            Qt::DirectConnection );
        QObject::connect(
            clientCtx.client.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
            clientCtx.client.get(),
            [&]( nx_http::AsyncHttpClientPtr client ) {
                clientCtx.multipartParser.processData( client->fetchMessageBodyBuffer() );
            },
            Qt::DirectConnection );
        clientCtx.client->doGet( url );
    }

    std::this_thread::sleep_for( std::chrono::seconds( 7 ) );

    clients.clear();
}

TEST_F(AsyncHttpClientTest, MultiRequestTest)
{
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test",
        "SimpleTest",
        "application/text"));
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test2",
        "SimpleTest2",
        "application/text"));
    ASSERT_TRUE(testHttpServer()->registerStaticProcessor(
        "/test3",
        "SimpleTest3",
        "application/text"));
    ASSERT_TRUE(testHttpServer()->bindAndListen());

    const QUrl url(lit("http://127.0.0.1:%1/test")
        .arg(testHttpServer()->serverAddress().port));
    const QUrl url2(lit("http://127.0.0.1:%1/test2")
        .arg(testHttpServer()->serverAddress().port));
    const QUrl url3(lit("http://127.0.0.1:%1/test3")
        .arg(testHttpServer()->serverAddress().port));

    auto client = nx_http::AsyncHttpClient::create();

    QByteArray expectedResponse;

    QnMutex mutex;
    QnWaitCondition waitCond;
    bool requestFinished = false;

    QObject::connect(
        client.get(), &nx_http::AsyncHttpClient::done,
        client.get(),
        [&](nx_http::AsyncHttpClientPtr client)
    {
        ASSERT_FALSE(client->failed());
        ASSERT_EQ(client->response()->statusLine.statusCode, nx_http::StatusCode::ok);
        ASSERT_EQ(client->fetchMessageBodyBuffer(), expectedResponse);
        auto contentTypeIter = client->response()->headers.find("Content-Type");
        ASSERT_TRUE(contentTypeIter != client->response()->headers.end());

        QnMutexLocker lock(&mutex);
        requestFinished = true;
        waitCond.wakeAll();
    }, Qt::DirectConnection);

    static const int kWaitTimeoutMs = 1000 * 10;
    // step 0: check if client reconnect smoothly if server already dropped connection
    // step 1: check 2 requests in a row via same connection
    for (int i = 0; i < 2; ++i)
    {
        testHttpServer()->setForceConnectionClose(i == 0);

        {
            QnMutexLocker lock(&mutex);
            requestFinished = false;
            expectedResponse = "SimpleTest";
            client->doGet(url);
            while (!requestFinished)
                ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));
        }

        {
            QnMutexLocker lock(&mutex);
            requestFinished = false;
            expectedResponse = "SimpleTest2";
            client->doGet(url2);
            while (!requestFinished)
                ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));
        }

        {
            QnMutexLocker lock(&mutex);
            requestFinished = false;
            expectedResponse = "SimpleTest3";
            client->doGet(url3);
            while (!requestFinished)
                ASSERT_TRUE(waitCond.wait(&mutex, kWaitTimeoutMs));
        }

    }
}

static QByteArray kBrockenResponse
(
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 100\r\n\r\n"
    "not enough content"
);

TEST_F(AsyncHttpClientTest, ConnectionBreak)
{
    nx::utils::promise<int> serverPort;
    std::thread serverThread(
        [&]()
        {
            const auto server = std::make_unique<nx::network::TCPServerSocket>(AF_INET);
            ASSERT_TRUE(server->bind(SocketAddress::anyAddress));
            ASSERT_TRUE(server->listen());
            serverPort.set_value(server->getLocalAddress().port);

            std::unique_ptr<AbstractStreamSocket> client(server->accept());
            ASSERT_TRUE((bool)client);

            QByteArray buffer(1024, Qt::Uninitialized);
            int size = 0;
            while (!buffer.contains(QByteArray("\r\n\r\n", 4)))
            {
                auto recv = client->recv(buffer.data() + size, buffer.size() - size);
                ASSERT_GT(recv, 0);
                size += recv;
            }

            buffer.resize(size);
            ASSERT_EQ(client->send(kBrockenResponse.data(), kBrockenResponse.size()), kBrockenResponse.size());
        });

    auto client = nx_http::AsyncHttpClient::create();
    nx::utils::promise<void> clientDone;
    QObject::connect(
        client.get(), &nx_http::AsyncHttpClient::done,
        [&](nx_http::AsyncHttpClientPtr client)
        {
            EXPECT_TRUE(client->failed());
            EXPECT_EQ(client->fetchMessageBodyBuffer(), QByteArray("not enough content"));
            EXPECT_EQ(client->lastSysErrorCode(), SystemError::connectionReset);
            clientDone.set_value();
        });

    client->doGet(lit("http://127.0.0.1:%1/test").arg(serverPort.get_future().get()));
    serverThread.join();
    clientDone.get_future().wait();
}

} // namespace nx_http
