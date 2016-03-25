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
#include <nx/network/test_support/socket_test_helper.h>
#include <utils/common/guard.h>
#include <utils/common/string.h>


namespace nx {
namespace network {
namespace test {

class SocketUdt
:
    public ::testing::Test
{
public:
    ~SocketUdt()
    {
        if (m_createStreamSocketFunc)
            SocketFactory::setCreateStreamSocketFunc(
                std::move(*m_createStreamSocketFunc));
        if (m_createStreamServerSocketFunc)
            SocketFactory::setCreateStreamServerSocketFunc(
                std::move(*m_createStreamServerSocketFunc));
    }

    void setCreateStreamSocketFunc(
        SocketFactory::CreateStreamSocketFuncType newFactoryFunc)
    {
        auto oldFunc = 
            SocketFactory::setCreateStreamSocketFunc(std::move(newFactoryFunc));
        if (!m_createStreamSocketFunc)
            m_createStreamSocketFunc = oldFunc;
    }

    void setCreateStreamServerSocketFunc(
        SocketFactory::CreateStreamServerSocketFuncType newFactoryFunc)
    {
        auto oldFunc =
            SocketFactory::setCreateStreamServerSocketFunc(std::move(newFactoryFunc));
        if (!m_createStreamServerSocketFunc)
            m_createStreamServerSocketFunc = oldFunc;
    }

private:
    boost::optional<SocketFactory::CreateStreamSocketFuncType> m_createStreamSocketFunc;
    boost::optional<SocketFactory::CreateStreamServerSocketFuncType> m_createStreamServerSocketFunc;
};


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

TEST_F(SocketUdt, cancelConnect)
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
        UdtStreamSocket sock;
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

    UdtStreamSocket sock;

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
    TEST_F, SocketUdt,
    &std::make_unique<UdtStreamServerSocket>,
    &std::make_unique<UdtStreamSocket>)

TEST_F(SocketUdt, rendezvousConnect)
{
    const std::chrono::milliseconds connectTimeout(2000);

    //creating two sockets, performing randezvous connect
    UdtStreamSocket connectorSocket;
    ASSERT_TRUE(connectorSocket.setRendezvous(true));
    ASSERT_TRUE(connectorSocket.setSendTimeout(connectTimeout.count()));
    ASSERT_TRUE(connectorSocket.setNonBlockingMode(true));
    ASSERT_TRUE(connectorSocket.bind(SocketAddress(HostAddress::localhost, 0)));

    UdtStreamSocket acceptorSocket;
    ASSERT_TRUE(acceptorSocket.setRendezvous(true));
    ASSERT_TRUE(acceptorSocket.setSendTimeout(connectTimeout.count()));
    ASSERT_TRUE(acceptorSocket.setNonBlockingMode(true));
    ASSERT_TRUE(acceptorSocket.bind(SocketAddress(HostAddress::localhost, 0)));

    auto socketStoppedGuard = makeScopedGuard(
        [&connectorSocket, &acceptorSocket]
        {
            //cleaning up
            connectorSocket.pleaseStopSync();
            acceptorSocket.pleaseStopSync();
        });

    std::promise<SystemError::ErrorCode> connectorConnectedPromise;
    connectorSocket.connectAsync(
        acceptorSocket.getLocalAddress(),
        [&connectorConnectedPromise](
            SystemError::ErrorCode errorCode)
        {
            connectorConnectedPromise.set_value(errorCode);
        });

    std::promise<SystemError::ErrorCode> acceptorConnectedPromise;
    acceptorSocket.connectAsync(
        connectorSocket.getLocalAddress(),
        [&acceptorConnectedPromise](
            SystemError::ErrorCode errorCode)
        {
            acceptorConnectedPromise.set_value(errorCode);
        });

    ASSERT_EQ(SystemError::noError, connectorConnectedPromise.get_future().get());
    ASSERT_EQ(SystemError::noError, acceptorConnectedPromise.get_future().get());

    //after successfull connect starting listener on one side and connector on the other one
    setCreateStreamSocketFunc(
        [](bool /*sslRequired*/, SocketFactory::NatTraversalType)
            -> std::unique_ptr<AbstractStreamSocket>
        {
            return std::make_unique<UdtStreamSocket>();
        });

    setCreateStreamServerSocketFunc(
        [](bool /*sslRequired*/, SocketFactory::NatTraversalType)
            -> std::unique_ptr<AbstractStreamServerSocket>
        {
            return std::make_unique<UdtStreamServerSocket>();
        });

    const size_t bytesToSendThroughConnection = 128*1024;
    const int maxSimultaneousConnections = 25;
    const std::chrono::seconds testDuration(3);

    RandomDataTcpServer server(bytesToSendThroughConnection);
    server.setLocalAddress(acceptorSocket.getLocalAddress());
    ASSERT_TRUE(server.start());

    ConnectionsGenerator connectionsGenerator(
        SocketAddress(QString::fromLatin1("localhost"), server.addressBeingListened().port),
        maxSimultaneousConnections,
        bytesToSendThroughConnection);
    connectionsGenerator.setLocalAddress(connectorSocket.getLocalAddress());
    connectionsGenerator.start();

    std::this_thread::sleep_for(testDuration);

    connectionsGenerator.pleaseStop();
    connectionsGenerator.join();

    server.pleaseStop();
    server.join();

    ASSERT_GT(connectionsGenerator.totalConnectionsEstablished(), 0);
    ASSERT_GT(connectionsGenerator.totalBytesSent(), 0);
    ASSERT_GT(connectionsGenerator.totalBytesReceived(), 0);
}

TEST_F(SocketUdt, acceptingFirstConnection)
{
    const std::chrono::seconds timeToWaitForSocketAccepted(1);
    const int loopLength = 71;

    for (int i = 0; i < loopLength; ++i)
    {
        UdtStreamServerSocket serverSocket;
        ASSERT_TRUE(serverSocket.bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_TRUE(serverSocket.listen());
        ASSERT_TRUE(serverSocket.setNonBlockingMode(true));

        std::promise<SystemError::ErrorCode> socketAcceptedPromise;
        serverSocket.acceptAsync(
            [&socketAcceptedPromise](
                SystemError::ErrorCode errorCode,
                AbstractStreamSocket* /*socket*/)
            {
                socketAcceptedPromise.set_value(errorCode);
            });

        //std::this_thread::sleep_for(std::chrono::milliseconds(100));

        UdtStreamSocket clientSock;
        ASSERT_TRUE(clientSock.connect(serverSocket.getLocalAddress()));

        auto future = socketAcceptedPromise.get_future();
        ASSERT_EQ(
            std::future_status::ready,
            future.wait_for(timeToWaitForSocketAccepted));
        ASSERT_EQ(SystemError::noError, future.get());

        serverSocket.pleaseStopSync();
    }
}

TEST_F(SocketUdt, DISABLED_allDataReadAfterFin)
{
    const int testMessageLength = 16;
    const std::chrono::milliseconds connectTimeout(3000);
    const std::chrono::milliseconds recvTimeout(3000);

    UdtStreamServerSocket server;
    //TCPServerSocket server;
    ASSERT_TRUE(server.setNonBlockingMode(true));
    ASSERT_TRUE(server.bind(SocketAddress(HostAddress::localhost, 0)));
    ASSERT_TRUE(server.listen());

    std::promise<
        std::pair<SystemError::ErrorCode, std::unique_ptr<AbstractStreamSocket>>
    > connectionAcceptedPromise;

    server.acceptAsync(
        [&connectionAcceptedPromise](
            SystemError::ErrorCode errorCode,
            AbstractStreamSocket* sock)
        {
            connectionAcceptedPromise.set_value(
                std::make_pair(errorCode, std::unique_ptr<AbstractStreamSocket>(sock)));
        });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto clientSock = std::make_unique<UdtStreamSocket>();
    //auto clientSock = std::make_unique<TCPSocket>();
    ASSERT_TRUE(clientSock->bind(SocketAddress(HostAddress::localhost, 0)));
    ASSERT_NE(server.getLocalAddress(), clientSock->getLocalAddress());

    ASSERT_TRUE(clientSock->connect(server.getLocalAddress()));

    auto connectionAcceptedFuture = connectionAcceptedPromise.get_future();
    ASSERT_EQ(
        std::future_status::ready,
        connectionAcceptedFuture.wait_for(connectTimeout));
    auto acceptResult = connectionAcceptedFuture.get();
    ASSERT_EQ(SystemError::noError, acceptResult.first);

    const QByteArray testMessage = generateRandomName(testMessageLength);
    ASSERT_EQ(
        testMessage.size(),
        clientSock->send(testMessage.constData(), testMessage.size()));

    clientSock.reset();

    //reading out accepted socket
    auto acceptedSocket = std::move(acceptResult.second);

    ASSERT_TRUE(acceptedSocket->setRecvTimeout(recvTimeout.count()))
        << SystemError::getLastOSErrorText().toStdString();

    const auto recvBuffer = readNBytes(acceptedSocket.get(), testMessage.size());
    ASSERT_EQ(testMessage, recvBuffer)
        << SystemError::getLastOSErrorText().toStdString();

    server.pleaseStopSync();
}

}   //test
}   //network
}   //nx
