#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

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

/** Creates \class FakeTcpTunnelConnection for each address */
struct FakeTcpTunnelAcceptor : AbstractTunnelAcceptor
{
    FakeTcpTunnelAcceptor(std::vector<SocketAddress> addresses)
        : m_addresses(std::move(addresses)) {}

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

/** Creates FakeTcpTunnelAcceptor instead of real ones */
struct CloudServerSocketTestCase : CloudServerSocket
{
    CloudServerSocketTestCase(IncomingTunnelPool* tunnelPool)
        : CloudServerSocket(nullptr, tunnelPool) {}

    bool listen(int) override
    {
        // TODO: #mux use multiple acceptors
        std::vector<SocketAddress> addresses;
        addresses.push_back(nx::network::test::kServerAddress);

        startAcceptor(
            std::make_unique<FakeTcpTunnelAcceptor>(std::move(addresses)),
            m_asyncGuard.sharedGuard());

        return true;
    }
};

struct CloudServerSocketTest
    : public ::testing::Test
{
protected:
    void TearDown() override
    {
        m_tunnelPool.pleaseStopSync();
    }

    IncomingTunnelPool m_tunnelPool;
};

// NOTE: sync tests are not working yet
NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, CloudServerSocketTest,
    [this](){ return std::make_unique<CloudServerSocketTestCase>(&m_tunnelPool); },
    &std::make_unique<TCPSocket>);

} // namespace cloud
} // namespace network
} // namespace nx
