/**********************************************************
* Feb, 1 2016
* a.kolesnikov
***********************************************************/

#include <chrono>

#include <gtest/gtest.h>

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/test_support/socket_test_helper.h>


namespace nx {
namespace network {
namespace cloud {

TEST(CloudStreamSocket, simple)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128*1024;

    //starting local tcp server
    test::RandomDataTcpServer server(bytesToSendThroughConnection);
    ASSERT_TRUE(server.start());

    const auto serverAddress = server.addressBeingListened();

    //registering local server address in AddressResolver
    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        tempHostName,
        serverAddress);

    //connecting with CloudStreamSocket to the local server
    CloudStreamSocket cloudSocket;
    ASSERT_TRUE(cloudSocket.connect(SocketAddress(tempHostName), 0));
    QByteArray data;
    data.resize(bytesToSendThroughConnection);
    const int bytesRead = cloudSocket.recv(data.data(), data.size(), MSG_WAITALL);
    ASSERT_EQ(bytesToSendThroughConnection, bytesRead);

    nx::network::SocketGlobals::addressResolver().removeFixedAddress(
        tempHostName,
        serverAddress);
}

TEST(CloudStreamSocket, general)
{
    const char* tempHostName = "bla.bla";
    const size_t bytesToSendThroughConnection = 128 * 1024;
    const size_t maxSimultaneousConnections = 1000;
    const std::chrono::seconds testDuration(3);

    SocketFactory::setCreateStreamSocketFunc(
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

} // namespace cloud
} // namespace network
} // namespace nx
