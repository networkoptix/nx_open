#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/cloud/cloud_server_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/test_support/stun_async_client_mock.h>

namespace nx {
namespace network {
namespace cloud {

const String kFakeLocalPeerId(QnUuid::createUuid().toSimpleString().toUtf8());
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
    FakeTcpTunnelAcceptor(boost::optional<SocketAddress> address):
        m_address(std::move(address)),
        m_ioThreadSocket(new TCPSocket)
    {
    }

    void accept(std::function<void(
        SystemError::ErrorCode,
        std::unique_ptr<AbstractTunnelConnection>)> handler) override
    {
        if (!m_address)
            return m_ioThreadSocket->registerTimer(
                100, [handler](){ handler(SystemError::timedOut, nullptr); });

        auto address = std::move(*m_address);
        m_address = boost::none;

        auto conn = std::make_unique<FakeTcpTunnelConnection>(std::move(address));
        handler(SystemError::noError, std::move(conn));
    }

    void pleaseStop(std::function<void()> handler) override
    {
        m_ioThreadSocket->pleaseStop(std::move(handler));
    }

    boost::optional<SocketAddress> m_address;
    std::unique_ptr<AbstractCommunicatingSocket> m_ioThreadSocket;
};

/** Creates @param nAcceptors @class FakeTcpTunnelAcceptor(s) instead of real ones */
template<quint16 nAcceptors>
struct CloudServerSocketTcpTester : CloudServerSocket
{
    CloudServerSocketTcpTester(): CloudServerSocket(nullptr) {}

    bool listen(int queueLen) override
    {
        initTunnelPool(queueLen);
        for (quint16 i = 0; i < nAcceptors; i++)
        {
            auto addr = nx::network::test::kServerAddress;
            startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(
                SocketAddress(addr.address, addr.port + i)));

            // also start useless acceptors
            startAcceptor(std::make_unique<FakeTcpTunnelAcceptor>(boost::none));
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
        return std::make_unique<ServerSocket>();
    }
};

struct CloudServerSocketSingleTcpTest
    : CloudServerSocketTcpTest<CloudServerSocketTcpTester<1>> {};

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, CloudServerSocketSingleTcpTest,
    [this](){ return makeServerSocket(); },
    &std::make_unique<TCPSocket>);

struct CloudServerSocketMultiTcpAcceptorTest
    : CloudServerSocketTcpTest<CloudServerSocketTcpTester<5>> {};

NX_NETWORK_SERVER_SOCKET_TEST_CASE(
    TEST_F, CloudServerSocketMultiTcpAcceptorTest,
    [this](){ return makeServerSocket(); },
    &std::make_unique<network::test::MultipleClientSocketTester<3>>);

void testTunnelConnect(SystemError::ErrorCode expectedResult)
{
    auto client = std::make_unique<TCPSocket>(false);
    ASSERT_TRUE(client->setNonBlockingMode(true));
    ASSERT_TRUE(client->setSendTimeout(500));

    std::promise<SystemError::ErrorCode> result;
    client->connectAsync(
        network::test::kServerAddress,
        [&](SystemError::ErrorCode c){ result.set_value(c); });

    EXPECT_EQ(result.get_future().get(), expectedResult);
    client->pleaseStopSync();
}

TEST(CloudServerSocketBaseTcpTest, OpenTunnelOnIndication)
{
    auto stunAsyncClient = std::make_shared<network::test::StunAsyncClientMock>();
    EXPECT_CALL(*stunAsyncClient, setIndicationHandler(
        stun::cc::indications::connectionRequested, ::testing::_)).Times(1);

    std::vector<CloudServerSocket::AcceptorMaker> acceptorMakers;
    acceptorMakers.push_back([](
        const String&, hpm::api::ConnectionRequestedEvent&)
    {
        return std::make_unique<FakeTcpTunnelAcceptor>(network::test::kServerAddress);
    });

    auto server = std::make_unique<CloudServerSocket>(
        std::make_shared<hpm::api::MediatorServerTcpConnection>(
            stunAsyncClient, nullptr),
        std::move(acceptorMakers));
    ASSERT_TRUE(server->listen(1));

    // there is not tunnel yet
    testTunnelConnect(SystemError::connectionRefused);

    stun::Message message(stun::Header(
        stun::MessageClass::indication, stun::cc::indications::connectionRequested));
    hpm::api::ConnectionRequestedEvent event;
    event.connectSessionID = String("someSessionId");
    event.originatingPeerID = String("somePeerId");
    event.serialize(&message);
    stunAsyncClient->emulateIndication(message);

    // now we can use estabilished tunnel
    testTunnelConnect(SystemError::noError);
}

} // namespace cloud
} // namespace network
} // namespace nx
