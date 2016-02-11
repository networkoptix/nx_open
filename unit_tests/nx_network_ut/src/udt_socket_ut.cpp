/**********************************************************
* Nov 24, 2015
* a.kolesnikov
***********************************************************/

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/network/udt/udt_pollset.h>
#include <nx/network/test_support/simple_socket_test_helper.h>


namespace nx {
namespace network {
namespace test {

class TestHandler
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    TestHandler(const nx_http::StringType& mimeType, QByteArray response)
    :
        m_mimeType(mimeType),
        m_response(std::move(response))
    {
    }

    virtual void processRequest(
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        const nx_http::Request& /*request*/,
        nx_http::Response* const /*response*/,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> completionHandler )
    {
        completionHandler(
            nx_http::StatusCode::ok,
            std::unique_ptr< nx_http::AbstractMsgBodySource >() );
    }

private:
    const nx_http::StringType m_mimeType;
    const QByteArray m_response;
};


namespace {
void onAcceptedConnection(
    AbstractStreamServerSocket* serverSocket,
    SystemError::ErrorCode /*errCode*/,
    AbstractStreamSocket* sock)
{
    delete sock;

    using namespace std::placeholders;
    serverSocket->acceptAsync(std::bind(onAcceptedConnection, serverSocket, _1, _2));
}
}

TEST(SocketUdt, cancelConnect)
{
    UdtStreamServerSocket serverSocket;
    ASSERT_TRUE(serverSocket.bind(SocketAddress(HostAddress::localhost, 0)));
    serverSocket.listen();
    using namespace std::placeholders;
    serverSocket.acceptAsync(std::bind(onAcceptedConnection, &serverSocket, _1, _2));

    //TestHttpServer testHttpServer;
    //ASSERT_TRUE(testHttpServer.bindAndListen());
    //testHttpServer.registerRequestProcessor< TestHandler >(
    //    "/test",
    //    []() -> std::unique_ptr< TestHandler >
    //    {
    //        return std::make_unique< TestHandler >(
    //            "application/octet-stream",
    //            QByteArray());
    //    });

    for (int i = 0; i < 100; ++i)
    {
        UdtStreamSocket sock(false);
        std::atomic<bool> handlerCalled(false);
        ASSERT_TRUE(sock.setNonBlockingMode(true));
        sock.connectAsync(
            serverSocket.getLocalAddress(),
            [&handlerCalled](SystemError::ErrorCode /*errorCode*/){
                handlerCalled = true;
            });
        sock.pleaseStopSync();

        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));
    }

    serverSocket.pleaseStopSync();
}

TEST(SocketUdt_UdtPollSet, general)
{
    UdtPollSet pollset;
    ASSERT_TRUE(pollset.isValid());

    UdtStreamSocket sock(false);

    ASSERT_TRUE(pollset.add(&sock, aio::etRead, (void*)1));
    ASSERT_TRUE(pollset.add(&sock, aio::etWrite, (void*)2));

    pollset.remove(&sock, aio::etRead);
    pollset.remove(&sock, aio::etWrite);
    ASSERT_TRUE(pollset.add(&sock, aio::etRead));
    pollset.remove(&sock, aio::etRead);

    auto result = pollset.poll(100);
    ASSERT_EQ(0, result);
}

NX_NETWORK_BOTH_SOCKETS_TEST_CASE(
    TEST, SocketUdt,
    &std::make_unique<UdtStreamServerSocket>,
    &std::make_unique<UdtStreamSocket>)

}   //test
}   //network
}   //nx
