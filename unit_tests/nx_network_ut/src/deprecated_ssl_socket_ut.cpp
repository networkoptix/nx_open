#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

namespace nx {
namespace network {
namespace deprecated {
namespace test {

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, DeprecatedSslFreeTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(std::make_unique<TCPServerSocket>(AF_INET), true); },
    [](){ return std::make_unique<TCPSocket>(AF_INET); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, DeprecatedSslEnabledTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(std::make_unique<TCPServerSocket>(AF_INET), true); },
    [](){ return std::make_unique<deprecated::SslSocket>(std::make_unique<TCPSocket>(AF_INET), false); });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, DeprecatedSslEnforcedTcpSockets,
    [](){ return std::make_unique<deprecated::SslServerSocket>(std::make_unique<TCPServerSocket>(AF_INET), false); },
    [](){ return std::make_unique<deprecated::SslSocket>(std::make_unique<TCPSocket>(AF_INET), false); });

namespace {

class BrokenTcpSocket:
    public TCPSocket
{
public:
    virtual bool setNonBlockingMode(bool /*val*/) override
    {
        SystemError::setLastErrorCode(SystemError::connectionReset);
        return false;
    }

    virtual bool getNonBlockingMode(bool* /*val*/) const override
    {
        SystemError::setLastErrorCode(SystemError::connectionReset);
        return false;
    }
};

} // namespace

class DeprecatedSslSocket:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
    }

    void givenSslSocketWithBrokenUnderlyingSocket()
    {
        m_sslSocket = std::make_unique<SslSocket>(
            std::make_unique<BrokenTcpSocket>(),
            false);
    }

    void whenReadSocketAsync()
    {
        m_readBuffer.clear();
        m_readBuffer.reserve(128);

        m_sslSocket->readSomeAsync(
            &m_readBuffer,
            [this](SystemError::ErrorCode sysErrorCode, std::size_t /*bytesRead*/)
            {
                m_readResults.push(sysErrorCode);
            });
    }

    void thenReadErrorIsReported()
    {
        ASSERT_NE(SystemError::noError, m_readResults.pop());
    }

    void thenGetNonBlockingModeFails()
    {
        bool val = false;
        ASSERT_FALSE(m_sslSocket->getNonBlockingMode(&val));
    }

    void thenSetNonBlockingModeFails()
    {
        ASSERT_FALSE(m_sslSocket->setNonBlockingMode(true));
    }

    void thenReadAsyncFails()
    {
        whenReadSocketAsync();
        thenReadErrorIsReported();
    }

private:
    std::unique_ptr<SslSocket> m_sslSocket;
    nx::Buffer m_readBuffer;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_readResults;
};

TEST_F(DeprecatedSslSocket, error_on_changing_underlying_socket_mode_is_properly_handled)
{
    givenSslSocketWithBrokenUnderlyingSocket();

    thenGetNonBlockingModeFails();
    thenSetNonBlockingModeFails();
    thenReadAsyncFails();
}

} // namespace test
} // namespace deprecated
} // namespace network
} // namespace nx
