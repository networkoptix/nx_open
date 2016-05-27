/**********************************************************
* Aug 25, 2015
* a.kolesnikov
***********************************************************/

#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <QString>

#include <nx/network/aio/timer.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <utils/common/cpp14.h>


namespace nx_http {

class HttpAsyncServerConnectionTest
:
    public ::testing::Test
{
public:
    HttpAsyncServerConnectionTest()
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
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> completionHandler )
    {
        m_timer.start(
            std::chrono::seconds(5),
            [completionHandler = std::move(completionHandler)]
            {
                completionHandler(
                    nx_http::StatusCode::ok,
                    std::unique_ptr<nx_http::AbstractMsgBodySource>());
            });
    }

private:
    nx::network::aio::Timer m_timer;
};

const QString TestHandler::PATH = "/tst";


TEST_F( HttpAsyncServerConnectionTest, connectionRemovedBeforeRequestHasBeenProcessed )
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
    client->doGet( url );
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
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> completionHandler )
    {
        response->headers.emplace(
            "Seq",
            nx_http::getHeaderValue( request.headers, "Seq" ) );
        completionHandler(
            nx_http::StatusCode::ok,
            std::make_unique<nx_http::BufferSource>("text/plain", "bla-bla-bla"));
    }
};

const nx::String PipeliningTestHandler::PATH = "/tst";


TEST_F(HttpAsyncServerConnectionTest, requestPipeliningTest )
{
    static const int REQUESTS_TO_SEND = 100;

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

    int msgCounter = 0;
    for( ; msgCounter < REQUESTS_TO_SEND; ++msgCounter )
    {
        nx_http::insertOrReplaceHeader(
            &request.headers,
            nx_http::HttpHeader( "Seq", nx::String::number( msgCounter ) ) );
        //sending request
        auto serializedMessage = request.serialized();
        ASSERT_EQ( sock->send( serializedMessage ), serializedMessage.size() );
    }

    //reading responses out of socket
    nx_http::HttpStreamReader httpMsgReader;

    nx::Buffer readBuf;
    readBuf.resize(4 * 1024 * 1024);
    int dataSize = 0;
    ASSERT_TRUE(sock->setRecvTimeout(500));
    while (msgCounter > 0)
    {
        if (dataSize == 0)
        {
            auto bytesRead = sock->recv(
                readBuf.data() + dataSize,
                readBuf.size() - dataSize );
            ASSERT_FALSE( bytesRead == 0 || bytesRead == -1 );
            dataSize += bytesRead;
        }

        size_t bytesParsed = 0;
        ASSERT_TRUE(
            httpMsgReader.parseBytes(
                QnByteArrayConstRef( readBuf, 0, dataSize ),
                &bytesParsed ) );
        readBuf.remove( 0, bytesParsed );
        dataSize -= bytesParsed;
        if( httpMsgReader.state() == nx_http::HttpStreamReader::messageDone )
        {
            ASSERT_TRUE( httpMsgReader.message().response != nullptr );
            auto seqIter = httpMsgReader.message().response->headers.find( "Seq" );
            ASSERT_TRUE( seqIter != httpMsgReader.message().response->headers.end() );
            ASSERT_EQ( seqIter->second.toInt(), REQUESTS_TO_SEND-msgCounter );
            --msgCounter;
        }
    }

    ASSERT_EQ( 0, msgCounter);
}

}
