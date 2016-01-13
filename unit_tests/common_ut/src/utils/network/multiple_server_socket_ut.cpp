#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>

#include "simple_socket_test_helper.h"

namespace nx {
namespace network {
namespace test {

class MultipleServerSocketTester
    : public MultipleServerSocket
{
public:
    MultipleServerSocketTester()
    {
        addSocket(std::make_unique<TCPServerSocket>());
        addSocket(std::make_unique<TCPServerSocket>());
        addSocket(std::make_unique<TCPServerSocket>());
    }

    bool bind(const SocketAddress& localAddress) override
    {
        auto port = localAddress.port;
        for(auto& socket : m_serverSockets)
        {
            SocketAddress modifiedAddress(localAddress.address, port++);
            NX_LOGX(lm("bind %1 to %2")
                    .arg(modifiedAddress.toString()).arg(&socket), cl_logDEBUG1);
            if (!socket->bind(modifiedAddress))
                return false;
        }

        return true;
    }
};

class MultipleClientSocketTester
    : public TCPSocket
{
public:
    MultipleClientSocketTester()
        : TCPSocket() {}

    bool connect(const SocketAddress& address, unsigned int timeout ) override
    {
        return TCPSocket::connect(modifyAddress(address), timeout);
    }

    void connectAsync(const SocketAddress& address,
                      std::function<void(SystemError::ErrorCode)> handler) override
    {
        TCPSocket::connectAsync(modifyAddress(address), std::move(handler));
    }

private:
    SocketAddress modifyAddress(const SocketAddress& address)
    {
        static quint16 modifier = 0;
        if (m_address == SocketAddress())
        {
            m_address = SocketAddress(address.address,
                                    address.port + (modifier++ % 3));
            NX_LOGX(lm("connect ") + m_address.toString(), cl_logDEBUG1);
        }

        return m_address;
    }

    SocketAddress m_address;
};

NX_SIMPLE_SOCKET_TESTS(MultipleServerSocket,
                       &std::make_unique<MultipleServerSocketTester>,
                       &std::make_unique<MultipleClientSocketTester>)

} // namespace test
} // namespace network
} // namespace nx
