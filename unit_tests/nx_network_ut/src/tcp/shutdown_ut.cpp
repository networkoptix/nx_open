#include <gtest/gtest.h>
#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/thread.h>
#include <nx/network/socket_common.h>

TEST(TcpSocketShutdownTest, DISABLED_main)
{
    auto socket = SocketFactory::createStreamSocket();
    socket->setRecvTimeout(1000 * 5);
    socket->setSendTimeout(1000 * 5);
    if (!socket->connect(SocketAddress("192.168.0.125", 554))) //< Any DW camera RTSP port
    {
        return; //< can't check
    }

    const auto startTime = std::chrono::steady_clock::now();
    std::future<void> asyncRead(std::async(
        [&]()
    {
        char buffer[1024];
        int bytesRead = socket->recv(buffer, sizeof(buffer));
        std::cout << bytesRead;
        int gg = bytesRead;
    }
    ));
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); //< wait for socket enter to a read operation
    socket->shutdown();
    asyncRead.wait();
    auto interval = std::chrono::steady_clock::now() - startTime;
    int timeoutMs = std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
    ASSERT_TRUE(timeoutMs < 1000);
}
