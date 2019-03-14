#pragma once

#include <gtest/gtest.h>

#include <nx/network/abstract_socket.h>
#include <nx/utils/std/future.h>

#include <nx/utils/system_error.h>

namespace nx {
namespace network {
namespace test {

template<typename ServerSocketType>
class ServerSocketTest:
    public ::testing::Test
{
public:
    ServerSocketTest():
        m_acceptResult(SystemError::noError)
    {
        init();
    }

    ~ServerSocketTest()
    {
        m_serverSocket.pleaseStopSync();
    }

protected:
    void whenMovedServerSocketToNonBlockingMode()
    {
        ASSERT_TRUE(m_serverSocket.setNonBlockingMode(false));
    }

    void whenCalledAcceptAsync()
    {
        nx::utils::promise<SystemError::ErrorCode> acceptDone;
        m_serverSocket.acceptAsync(
            [&acceptDone](
                SystemError::ErrorCode sysErrorCode,
                std::unique_ptr<AbstractStreamSocket> /*socket*/)
            {
                acceptDone.set_value(sysErrorCode);
            });
        m_acceptResult = acceptDone.get_future().get();
    }

    void thenErrorShouldBeReported()
    {
        ASSERT_EQ(SystemError::notSupported, m_acceptResult);
    }

private:
    ServerSocketType m_serverSocket;
    SystemError::ErrorCode m_acceptResult;

    void init()
    {
        ASSERT_TRUE(m_serverSocket.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_serverSocket.listen());
    }
};

TYPED_TEST_CASE_P(ServerSocketTest);

TYPED_TEST_P(ServerSocketTest, accept_async_on_blocking_socket_results_in_error)
{
    this->whenMovedServerSocketToNonBlockingMode();
    this->whenCalledAcceptAsync();
    this->thenErrorShouldBeReported();
}

REGISTER_TYPED_TEST_CASE_P(ServerSocketTest, accept_async_on_blocking_socket_results_in_error);

} // namespace test
} // namespace network
} // namespace nx
