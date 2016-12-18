#include <gtest/gtest.h>

#include <thread>

#include <nx/network/cloud/tunnel/tcp/reverse_connector.h>
#include <nx/network/system_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {
namespace test {

static std::vector<Buffer> kRequestLines
{
    "OPTIONS * HTTP/1.1",
    "Host: 127.0.0.1",
    "Upgrade: NXRC/1.0",
    "Connection: Upgrade",
    "Nxrc-Host-Name: client",
};

static std::vector<Buffer> kResponseLines
{
    "HTTP/1.1 101 Switching Protocols",
    "Upgrade: NXRC/1.0, HTTP/1.1",
    "Connection: Upgrade",
    "Nxrc-Host-Name: server",
    "Nxrc-Pool-Size: 5",
    "Nxrc-Keep-Alive-Options: { 1, 2, 3 }",
};

static const Buffer kDelimiter = "\r\n";

static Buffer httpJoin(std::vector<Buffer> lines, Buffer append = {})
{
    Buffer result;
    for (const auto& line: lines)
        result.append(line + kDelimiter);

    result.append(kDelimiter + append);
    return result;
}

TEST(TcpReverseConnector, General)
{
    utils::promise<SocketAddress> serverAddress;
    std::thread serverThread(
        [this, &serverAddress]()
        {
            const auto server = std::make_unique<TCPServerSocket>(AF_INET);
            ASSERT_TRUE(server->bind(SocketAddress::anyAddress));
            ASSERT_TRUE(server->listen());
            serverAddress.set_value(server->getLocalAddress());

            std::unique_ptr<AbstractStreamSocket> client(server->accept());
            ASSERT_TRUE((bool)client);

            Buffer buffer(1024, Qt::Uninitialized);
            int size = 0;
            while (!buffer.left(size).contains(kDelimiter + kDelimiter))
            {
                auto recv = client->recv(buffer.data() + size, buffer.size() - size);
                ASSERT_GT(recv, 0);
                size += recv;
            }

            buffer.resize(size);
            for (const auto line: kRequestLines)
                ASSERT_TRUE(buffer.contains(line));

            const auto response = httpJoin(kResponseLines, "hello");
            ASSERT_EQ(client->send(response.data(), response.size()), response.size());
        });

    auto connector = std::make_unique<ReverseConnector>("client", "server", nullptr);
    utils::promise<void> connectorDone;
    connector->connect(
        SocketAddress(HostAddress::localhost, serverAddress.get_future().get().port),
        [&connector, &connectorDone](SystemError::ErrorCode code)
        {
            auto buffer = std::make_shared<Buffer>();
            buffer->reserve(100);
            ASSERT_EQ(code, SystemError::noError);
            ASSERT_EQ(connector->getPoolSize(), boost::make_optional<size_t>(5));
            ASSERT_EQ(connector->getKeepAliveOptions(), KeepAliveOptions(1, 2, 3));

            std::shared_ptr<AbstractStreamSocket> socket(connector->takeSocket().release());
            auto socketRaw = socket.get();
            socketRaw->readAsyncAtLeast(
                buffer.get(), 5,
                [buffer, &connectorDone, socket = std::move(socket)](
                    SystemError::ErrorCode code, size_t) mutable
                {
                    ASSERT_EQ(code, SystemError::noError);
                    ASSERT_EQ(*buffer, Buffer("hello"));

                    socket.reset();
                    connectorDone.set_value();
                });
        });

    serverThread.join();
    connectorDone.get_future().wait();
}

} // namespace test
} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
