/**********************************************************
* Feb, 1 2016
* a.kolesnikov
***********************************************************/

#include <chrono>

#include <gtest/gtest.h>

#include <utils/common/cpp14.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/test_support/simple_socket_test_helper.h>


namespace nx {
namespace network {
namespace cloud {

class CloudStreamSocketTest
:
    public ::testing::Test
{
public:
    ~CloudStreamSocketTest()
    {
        if (m_oldCreateStreamSocketFunc)
            SocketFactory::setCreateStreamSocketFunc(
                std::move(*m_oldCreateStreamSocketFunc));
    }

    void setCreateStreamSocketFunc(
        SocketFactory::CreateStreamSocketFuncType newFactoryFunc)
    {
        auto oldFunc = 
            SocketFactory::setCreateStreamSocketFunc(std::move(newFactoryFunc));
        if (!m_oldCreateStreamSocketFunc)
            m_oldCreateStreamSocketFunc = std::move(oldFunc);
    }

private:
    boost::optional<SocketFactory::CreateStreamSocketFuncType> 
        m_oldCreateStreamSocketFunc;
};

TEST_F(CloudStreamSocketTest, simple)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128*1024;
    const size_t repeatCount = 100;

    //starting local tcp server
    test::RandomDataTcpServer server(bytesToSendThroughConnection);
    ASSERT_TRUE(server.start());

    const auto serverAddress = server.addressBeingListened();

    //registering local server address in AddressResolver
    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    for (int i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket;
        ASSERT_TRUE(cloudSocket.connect(SocketAddress(tempHostName), 0));
        QByteArray data;
        data.resize(bytesToSendThroughConnection);
        const int bytesRead = cloudSocket.recv(data.data(), data.size(), MSG_WAITALL);
        ASSERT_EQ(bytesToSendThroughConnection, bytesRead);
    }

    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        tempHostName,
        serverAddress);
}

TEST_F(CloudStreamSocketTest, multiple_connections_random_data)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128 * 1024;
    const size_t maxSimultaneousConnections = 1000;
    const std::chrono::seconds testDuration(3);

    setCreateStreamSocketFunc(
        []( bool /*sslRequired*/,
            SocketFactory::NatTraversalType /*natTraversalRequired*/) ->
                std::unique_ptr< AbstractStreamSocket >
        {
            return std::make_unique<CloudStreamSocket>();
        });

    //starting local tcp server
    test::RandomDataTcpServer server(bytesToSendThroughConnection);
    ASSERT_TRUE(server.start());

    const auto serverAddress = server.addressBeingListened();

    //registering local server address in AddressResolver
    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    test::ConnectionsGenerator connectionsGenerator(
        SocketAddress(tempHostName),
        maxSimultaneousConnections,
        bytesToSendThroughConnection);
    connectionsGenerator.start();

    std::this_thread::sleep_for(testDuration);

    connectionsGenerator.pleaseStop();
    connectionsGenerator.join();

    server.pleaseStop();
    server.join();

    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        tempHostName,
        serverAddress);
}

const auto createServerSocketFunc = 
    []() -> std::unique_ptr<AbstractStreamServerSocket>
    {
        return SocketFactory::createStreamServerSocket();
    };
const auto createClientSocketFunc =
    []() -> std::unique_ptr<CloudStreamSocket>
    {
        return std::make_unique<CloudStreamSocket>();
    };

TEST_F(CloudStreamSocketTest, simple_socket_test)
{
    const char* tempHostName = "bla.bla";
    SocketAddress serverAddress(HostAddress::localhost, 20000 + (rand()%32000));

    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    test::socketSimpleSync(
        createServerSocketFunc,
        createClientSocketFunc,
        serverAddress,
        SocketAddress(tempHostName));

    test::socketSimpleAsync(
        createServerSocketFunc,
        createClientSocketFunc,
        serverAddress,
        SocketAddress(tempHostName));

    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        tempHostName,
        serverAddress);
}

TEST_F(CloudStreamSocketTest, shutdown)
{
    const char* tempHostName = "bla.bla";
    SocketAddress serverAddress(HostAddress::localhost, 20000 + (rand() % 32000));

    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    test::shutdownSocket(
        createServerSocketFunc,
        createClientSocketFunc,
        serverAddress,
        SocketAddress(tempHostName));

    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        tempHostName,
        serverAddress);
}

TEST_F(CloudStreamSocketTest, cancellation)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128 * 1024;
    const size_t repeatCount = 100;

    //starting local tcp server
    test::RandomDataTcpServer server(bytesToSendThroughConnection);
    ASSERT_TRUE(server.start());

    const auto serverAddress = server.addressBeingListened();

    //registering local server address in AddressResolver
    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    //cancelling connect
    for (int i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket;
        cloudSocket.connectAsync(
            SocketAddress(tempHostName),
            [](SystemError::ErrorCode /*code*/){});
        cloudSocket.cancelIOSync(aio::etNone);
    }

    //cancelling read
    for (int i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket;
        ASSERT_TRUE(cloudSocket.connect(
            SocketAddress(tempHostName),
            std::chrono::milliseconds(1000).count()));
        QByteArray data;
        data.reserve(bytesToSendThroughConnection);
        cloudSocket.readSomeAsync(
            &data,
            [](SystemError::ErrorCode /*errorCode*/, size_t /*bytesRead*/) {});
        cloudSocket.cancelIOSync(aio::etNone);
    }

    //cancelling send
    for (int i = 0; i < repeatCount; ++i)
    {
        //connecting with CloudStreamSocket to the local server
        CloudStreamSocket cloudSocket;
        ASSERT_TRUE(cloudSocket.connect(
            SocketAddress(tempHostName),
            std::chrono::milliseconds(1000).count()));
        QByteArray data;
        data.resize(bytesToSendThroughConnection);
        cloudSocket.sendAsync(
            data,
            [](SystemError::ErrorCode /*errorCode*/, size_t /*bytesSent*/) {});
        cloudSocket.cancelIOSync(aio::etNone);
    }

    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        tempHostName,
        serverAddress);
}

} // namespace cloud
} // namespace network
} // namespace nx
