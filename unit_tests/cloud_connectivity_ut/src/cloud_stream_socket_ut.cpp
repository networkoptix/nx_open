
#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/test_support/socket_test_helper.h>


namespace nx {
namespace network {
namespace cloud {

TEST(CloudStreamSocket, general)
{
    const size_t kBytesSentByServer = 128*1024;

    //starting local tcp server
    test::RandomDataTcpServer server(kBytesSentByServer);
    ASSERT_TRUE(server.start());

    //registering local server address in AddressResolver

    //connecting with CloudStreamSocket to local server
}

} // namespace cloud
} // namespace network
} // namespace nx
