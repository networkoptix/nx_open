#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>

namespace nx {
namespace network {
namespace cloud {

const String kFakeRemotePeerId(QnUuid::createUuid().toSimpleString().toUtf8());

/** Accepts usual TCP connections */
struct FakeTcpTunnelConnection : AbstractTunnelConnection
{
    FakeTcpTunnelConnection(SocketAddress address)
        : AbstractTunnelConnection(kFakeRemotePeerId)
        , m_server(std::make_unique<TCPServerSocket>())
    {
        assert(m_server->setNonBlockingMode(true));
        assert(m_server->setReuseAddrFlag(true));
        assert(m_server->bind(address));
        assert(m_server->listen());
    }

    void establishNewConnection(
        boost::optional<std::chrono::milliseconds>,
        SocketAttributes, SocketHandler) override
    {
        // not supposed to be used for accept
        assert(false);
    }

    void accept(SocketHandler handler) override
    {
        m_server->acceptAsync([handler](
            SystemError::ErrorCode c, AbstractStreamSocket* s)
        {
            handler(c, std::unique_ptr<AbstractStreamSocket>(s), true);
        });
    }

    void pleaseStop(std::function<void()> handler) override
    {
        m_server->pleaseStop([this, handler]
        {
            m_server.reset();
            handler();
        });
    }

    std::unique_ptr<AbstractStreamServerSocket> m_server;
};

/** Creates several \class FakeTcpTunnelConnection */
struct FakeTcpTunnelAcceptor : AbstractTunnelAcceptor
{
    FakeTcpTunnelAcceptor(SocketAddress address)
    {
        m_addresses.push_back(std::move(address));
    }

    void accept(
        std::function<void(std::unique_ptr<AbstractTunnelConnection>)> handler) override
    {
        if (!m_addresses.empty())
        {
            auto address = m_addresses.back();
            m_addresses.pop_back();
            handler(std::make_unique<FakeTcpTunnelConnection>(std::move(address)));
        }
    }

    void pleaseStop(std::function<void()> handler) override
    {
        handler();
    }

    std::vector<SocketAddress> m_addresses;
};

/** Creates @param kShift @class FakeTcpTunnelAcceptor(s) instead of real ones */
template<quint16 nAcceptors>
struct CloudServerSocketTcpTester : CloudServerSocket
{
    CloudServerSocketTcpTester(IncomingTunnelPool* tunnelPool)
        : CloudServerSocket(nullptr, tunnelPool) {}

    bool listen(int queueLen) override
    {
        m_tunnelPool->setAcceptLimit(queueLen);
        for (quint16 i = 0; i < nAcceptors; i++)
        {
            auto addr = nx::network::test::kServerAddress;
            startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                SocketAddress(addr.address, addr.port + i)));
        }

        return true;
    }
};

template<typename ServerSocket>
struct CloudServerSocketTcpTest
    : public ::testing::Test
{
protected:
    std::unique_ptr<ServerSocket> makeServerSocket()
    {
        return std::make_unique<ServerSocket>(&m_tunnelPool);
    }

    void TearDown() override
    {
        m_tunnelPool.pleaseStopSync();
    }

    IncomingTunnelPool m_tunnelPool;
};

struct CloudServerSocketSingleTcpTest
    : CloudServerSocketTcpTest<CloudServerSocketTcpTester<1>> {};

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, CloudServerSocketSingleTcpTest,
    [this](){ return makeServerSocket(); },
    &std::make_unique<TCPSocket>);

struct CloudServerSocketMultiTcpAcceptorTest
    : CloudServerSocketTcpTest<CloudServerSocketTcpTester<4>> {};

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, CloudServerSocketMultiTcpAcceptorTest,
    [this](){ return makeServerSocket(); },
    &std::make_unique<network::test::MultipleClientSocketTester<3>>);

} // namespace cloud
} // namespace network
} // namespace nx
