#include <memory>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/ssl/ssl_stream_server_socket.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>

namespace nx {
namespace network {
namespace ssl {
namespace test {

class StreamServerSocket:
    public ::testing::Test
{
public:
    StreamServerSocket():
        m_httpServer(
            std::make_unique<ssl::StreamServerSocket>(
                std::make_unique<TCPServerSocket>(AF_INET),
                EncryptionUse::autoDetectByReceivedData))
    {
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void whenEstablishEncryptedConnection()
    {
        m_clientSocket = std::make_unique<ssl::ClientStreamSocket>(
            std::make_unique<TCPSocket>(AF_INET));
        ASSERT_TRUE(m_clientSocket->connect(m_httpServer.serverAddress(), kNoTimeout));
    }

    void whenEstablishNotEncryptedConnection()
    {
        m_clientSocket = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(m_clientSocket->connect(m_httpServer.serverAddress(), kNoTimeout));
    }

    void thenConnectionIsAccepted()
    {
        // TODO
    }

    void andConnectionCanBeUsed()
    {
        ASSERT_GT(m_clientSocket->send("GET / HTTP/1.1\r\n\r\n"), 0);

        std::array<char, 128> readBuf;
        ASSERT_GT(m_clientSocket->recv(readBuf.data(), (int) readBuf.size()), 0);

        const std::string httpResponsePrefix("HTTP/1.1");
        ASSERT_TRUE(std::equal(
            httpResponsePrefix.begin(), httpResponsePrefix.end(),
            readBuf.data()));
    }

private:
    http::TestHttpServer m_httpServer;
    std::unique_ptr<AbstractStreamSocket> m_clientSocket;
};

TEST_F(StreamServerSocket, accepts_ssl_connections)
{
    whenEstablishEncryptedConnection();

    thenConnectionIsAccepted();
    andConnectionCanBeUsed();
}

TEST_F(StreamServerSocket, accepts_non_ssl_connections)
{
    whenEstablishNotEncryptedConnection();

    thenConnectionIsAccepted();
    andConnectionCanBeUsed();
}

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
