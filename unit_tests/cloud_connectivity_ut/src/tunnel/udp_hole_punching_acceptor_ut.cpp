
#include <gtest/gtest.h>

#include <random>
#include <thread>

#include <nx/network/cloud/tunnel/udp/acceptor.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/udp_server.h>
#include <nx/network/test_support/stun_async_client_mock.h>
#include <utils/thread/sync_queue.h>


namespace nx {
namespace network {
namespace cloud {
namespace udp {
namespace test {

const SocketAddress kMediatorAddress("127.0.0.1:12345");
const SocketAddress k2ndPeerAddress("127.0.0.1:12346");
const String kRemotePeerId("SomePeerId");
const String kConnectionSessionId("SomeSessionId");
const std::chrono::milliseconds kSocketTimeout(2000);
const std::chrono::milliseconds kUdpRetryTimeout(500);
const size_t kPleaseStopRunCount(10);

class DummyCloudSystemCredentialsProvider
:
    public hpm::api::AbstractCloudSystemCredentialsProvider
{
    boost::optional<hpm::api::SystemCredentials> getSystemCredentials() const override
    {
        hpm::api::SystemCredentials c;
        c.serverId = String("SomeServerId");
        c.systemId = String("SomeSystemId");
        c.key = String("SomeAuthKey");
        return std::move(c);
    }
}
dummyCloudSystemCredentialsProvider;

class UdpHolePunchingTunnelAcceptorTest
:
    public ::testing::Test
{
protected:
    UdpHolePunchingTunnelAcceptorTest()
    :
        stunClientMock(std::make_shared<network::test::StunAsyncClientMock>()),
        manualAcceptorStop(false),
        udpStunServer(&stunMessageDispatcher),
        isUdpServerEnabled(true),
        connectionRequests(0)
    {
        EXPECT_CALL(*stunClientMock, remoteAddress())
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Return(kMediatorAddress));

        stunMessageDispatcher.registerRequestProcessor(
            stun::cc::methods::connectionAck,
            [this](
                std::shared_ptr<stun::AbstractServerConnection> c,
                stun::Message m)
            {
                onUdpRequest(std::move(c), std::move(m));
            });

        NX_ASSERT(udpStunServer.bind(kMediatorAddress));
        NX_ASSERT(udpStunServer.listen());
    }

    void createAndStartAcceptor(size_t socketsToAccept)
    {
        if (mediatorConnection)
            mediatorConnection->pleaseStopSync();

        mediatorConnection.reset(new hpm::api::MediatorServerTcpConnection(
            stunClientMock, &dummyCloudSystemCredentialsProvider));

        nx::hpm::api::ConnectionParameters connectionParameters;
        connectionParameters.rendezvousConnectTimeout = kSocketTimeout;
        tunnelAcceptor.reset(new TunnelAcceptor(k2ndPeerAddress, connectionParameters));
        tunnelAcceptor->setConnectionInfo(kConnectionSessionId, kRemotePeerId);
        tunnelAcceptor->setMediatorConnection(mediatorConnection);
        tunnelAcceptor->setUdpRetransmissionTimeout(kUdpRetryTimeout);
        tunnelAcceptor->setUdpMaxRetransmissions(1);

        tunnelAcceptor->accept(
            [this, socketsToAccept](
                SystemError::ErrorCode code,
                std::unique_ptr<AbstractIncomingTunnelConnection> newConnection)
            {
                if (!manualAcceptorStop)
                    tunnelAcceptor.reset();

                acceptResults.push(code);
                if (code != SystemError::noError)
                    return;

                tunnelConnection = std::move(newConnection);
                acceptOnConnection(socketsToAccept);
            });
    }

    void acceptOnConnection(size_t socketsToAccept)
    {
        tunnelConnection->accept(
            [this, socketsToAccept](
                SystemError::ErrorCode code,
                std::unique_ptr<AbstractStreamSocket> socket)
            {
                acceptedSockets.push_back(std::move(socket));
                acceptResults.push(code);

                if (code == SystemError::noError && socketsToAccept > 1)
                    acceptOnConnection(socketsToAccept - 1);
                else
                    tunnelConnection.reset();
            });
    }

    void onUdpRequest(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        stun::Message message)
    {
        if (!isUdpServerEnabled)
        {
            NX_LOGX(lm("UDT server is disabled, ignore message"), cl_logDEBUG2);
            return;
        }

        nx::hpm::api::ConnectionAckRequest ack;
        ASSERT_TRUE(ack.parse(message));
        ASSERT_EQ(ack.connectSessionId, kConnectionSessionId);

        SocketAddress udtAddress = connection->getSourceAddress();
        NX_LOGX(lm("Got connectionAck from %1")
            .arg(udtAddress.toString()), cl_logALWAYS);

        NX_LOGX(lm("Initiate rendevous UDT connection from %1 to %2")
            .arg(k2ndPeerAddress.toString())
            .arg(udtAddress.toString()), cl_logDEBUG2);

        if (connectionRequests)
            connectControlSocket(udtAddress);

        stun::Message response(stun::Header(
            stun::MessageClass::successResponse,
            stun::cc::methods::connectionAck,
            std::move(message.header.transactionId)));
        response.newAttribute<stun::cc::attrs::ResultCode>(
            hpm::api::ResultCode::ok);
        connection->sendMessage(std::move(response));
    }

    void connectControlSocket(const SocketAddress& address)
    {
        auto socket = std::make_unique<UdtStreamSocket>();
        ASSERT_TRUE(socket->setRendezvous(true));
        ASSERT_TRUE(socket->setSendTimeout(kSocketTimeout.count()));
        ASSERT_TRUE(socket->setNonBlockingMode(true));
        ASSERT_TRUE(socket->bind(k2ndPeerAddress));
        socket->connectAsync(
            address,
            [this, address](SystemError::ErrorCode code)
            {
                NX_LOGX(lm("Rendevous UDT connection from %1 to %2: %3")
                    .arg(k2ndPeerAddress.toString()).arg(address.toString())
                    .arg(SystemError::toString(code)), cl_logDEBUG1);

                ASSERT_EQ(code, SystemError::noError);
                connectClientSocket(address);
            });

        connectSockets.push_back(std::move(socket));
    }

    void connectClientSocket(const SocketAddress& address)
    {
        auto socket = std::make_unique<UdtStreamSocket>();
        ASSERT_TRUE(socket->setSendTimeout(kSocketTimeout.count()));
        ASSERT_TRUE(socket->setNonBlockingMode(true));
        socket->connectAsync(
            address,
            [this, address](SystemError::ErrorCode code)
            {
                connectResults.push(code);
                NX_LOG(lm("Client UDT connect to %1: %2")
                    .arg(address.toString())
                    .arg(SystemError::toString(code)), cl_logDEBUG1);

                --connectionRequests;
                if (code == SystemError::noError && connectionRequests)
                    connectClientSocket(address);
            });

        connectSockets.push_back(std::move(socket));
    }

    void TearDown() override
    {
        for (auto& socket : connectSockets)
            socket->pleaseStopSync();

        if (tunnelAcceptor)
            tunnelAcceptor->pleaseStopSync();

        if (mediatorConnection)
            mediatorConnection->pleaseStopSync();
    }

    std::shared_ptr<network::test::StunAsyncClientMock> stunClientMock;
    std::shared_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection;

    bool manualAcceptorStop;
    std::unique_ptr<TunnelAcceptor> tunnelAcceptor;
    stun::MessageDispatcher stunMessageDispatcher;
    stun::UDPServer udpStunServer;

    bool isUdpServerEnabled;
    size_t connectionRequests;
    TestSyncQueue<SystemError::ErrorCode> connectResults;
    std::vector<std::unique_ptr<AbstractStreamSocket>> connectSockets;

    TestSyncQueue<SystemError::ErrorCode> acceptResults;
    std::unique_ptr<AbstractIncomingTunnelConnection> tunnelConnection;
    std::vector<std::unique_ptr<AbstractStreamSocket>> acceptedSockets;
};

TEST_F(UdpHolePunchingTunnelAcceptorTest, UdpRequestTimeout)
{
    isUdpServerEnabled = false;
    createAndStartAcceptor(1);
    ASSERT_EQ(acceptResults.pop(), SystemError::connectionAbort); // connection
}

TEST_F(UdpHolePunchingTunnelAcceptorTest, UdpRequestPleaseStop)
{
    isUdpServerEnabled = false;
    manualAcceptorStop = true;
    createAndStartAcceptor(1);
    tunnelAcceptor->pleaseStopSync();
    tunnelAcceptor.reset();
}

TEST_F(UdpHolePunchingTunnelAcceptorTest, UdtConnectTimeout)
{
    createAndStartAcceptor(1);
    ASSERT_EQ(acceptResults.pop(), SystemError::timedOut); // connection
}

TEST_F(UdpHolePunchingTunnelAcceptorTest, ConnectPleaseStop)
{
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int64_t> distribution(0, kUdpRetryTimeout.count() * 2);
    for (size_t i = 0; i < kPleaseStopRunCount; ++i)
    {
        const auto delay = distribution(generator);
        NX_LOG(lm("== %1 == delay: %2 ms").arg(i).arg(delay), cl_logALWAYS);

        createAndStartAcceptor(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        tunnelAcceptor->pleaseStopSync();
        tunnelAcceptor.reset();
        ASSERT_TRUE(acceptResults.isEmpty());
    }
}

TEST_F(UdpHolePunchingTunnelAcceptorTest, SingleUdtConnect)
{
    connectionRequests = 1;
    createAndStartAcceptor(1);
    ASSERT_EQ(acceptResults.pop(), SystemError::noError); // connection
    ASSERT_EQ(acceptResults.pop(), SystemError::noError); // socket
    ASSERT_EQ(connectResults.pop(), SystemError::noError);
}

TEST_F(UdpHolePunchingTunnelAcceptorTest, MultiUdtConnect)
{
    connectionRequests = 5;
    createAndStartAcceptor(5);
    ASSERT_EQ(acceptResults.pop(), SystemError::noError); // connection
    for (size_t i = 0; i < 5; ++i)
    {
        ASSERT_EQ(acceptResults.pop(), SystemError::noError) 
            << "i = " << i; // socket
        ASSERT_EQ(connectResults.pop(), SystemError::noError);
    }
}

} // namespace test
} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
