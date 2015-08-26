/**********************************************************
* Aug 25, 2015
* a.kolesnikov
***********************************************************/

#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <QString>

#include <utils/common/cpp14.h>
#include <utils/network/http/server/abstract_http_request_handler.h>
#include <utils/network/http/httpclient.h>

#include "../test_http_server.h"


namespace nx_http {

class AsyncServerConnectionTest
:
    public ::testing::Test
{
public:
    AsyncServerConnectionTest()
    :
        m_testHttpServer( std::make_unique<TestHttpServer>() )
    {
    }

protected:
    std::unique_ptr<TestHttpServer> m_testHttpServer;
};


//!Delays response for several seconds
class TestHandler
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    static const QString PATH;

    virtual ~TestHandler()
    {
    }

    //!Implementation of \a nx_http::AbstractHttpRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        stree::ResourceContainer&& authInfo,
        const nx_http::Request& request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler )
    {
        std::async(
            std::launch::async,
            [completionHandler]() {
                std::this_thread::sleep_for( std::chrono::seconds(5) );
                completionHandler(
                    nx_http::StatusCode::ok,
                    std::unique_ptr<nx_http::AbstractMsgBodySource>() );
            } );
    }
};

const QString TestHandler::PATH = "/tst";


TEST_F( AsyncServerConnectionTest, connectionRemovedBeforeRequestHasBeenProcessed )
{
    m_testHttpServer->registerRequestProcessor<TestHandler>(
        TestHandler::PATH,
        [&]()->std::unique_ptr<TestHandler> {
            return std::make_unique<TestHandler>();
        } );

    ASSERT_TRUE( m_testHttpServer->bindAndListen() );

    //fetching mjpeg with async http client
    const QUrl url( QString( "http://127.0.0.1:%1%2" ).
        arg( m_testHttpServer->serverAddress().port ).arg( TestHandler::PATH ) );

    auto client = nx_http::AsyncHttpClient::create();
    ASSERT_TRUE( client->doGet( url ) );
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    client.reset();

    std::this_thread::sleep_for( std::chrono::seconds( 10 ) );
}


//!Delays response for several seconds
class PipeliningTestHandler
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    static const nx::String PATH;

    PipeliningTestHandler()
    {
    }

    virtual ~PipeliningTestHandler()
    {
    }

    //!Implementation of \a nx_http::AbstractHttpRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        stree::ResourceContainer&& authInfo,
        const nx_http::Request& request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler )
    {
        response->headers.emplace(
            "Seq",
            nx_http::getHeaderValue( request.headers, "Seq" ) );
        completionHandler(
            nx_http::StatusCode::ok,
            std::unique_ptr<nx_http::AbstractMsgBodySource>() );
    }
};

const nx::String PipeliningTestHandler::PATH = "/tst";


TEST_F( AsyncServerConnectionTest, requestPipeliningTest )
{
    static const int REQUESTS_TO_SEND = 10;

    m_testHttpServer->registerRequestProcessor<PipeliningTestHandler>(
        PipeliningTestHandler::PATH,
        [&]()->std::unique_ptr<PipeliningTestHandler> {
            return std::make_unique<PipeliningTestHandler>();
        } );

    ASSERT_TRUE( m_testHttpServer->bindAndListen() );

    //opening connection and sending multiple requests
    nx_http::Request request;
    request.requestLine.method = nx_http::Method::GET;
    request.requestLine.version = nx_http::http_1_1;
    request.requestLine.url = PipeliningTestHandler::PATH;

    auto sock = SocketFactory::createStreamSocket();
    ASSERT_TRUE( sock->connect( SocketAddress(
        HostAddress::localhost,
        m_testHttpServer->serverAddress().port ) ) );

    for( int i = 0; i < REQUESTS_TO_SEND; ++i )
    {
        nx_http::insertOrReplaceHeader(
            &request.headers,
            nx_http::HttpHeader( "Seq", nx::String::number(i) ) );
        //sending request
        auto serializedMessage = request.serialized();
        ASSERT_EQ( sock->send( serializedMessage ), serializedMessage.size() );
    }

    //TODO #ak reading responses out of socket
}

}
