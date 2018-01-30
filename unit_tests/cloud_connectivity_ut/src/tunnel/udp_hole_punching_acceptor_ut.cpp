#include <gtest/gtest.h>

#include <random>
#include <thread>

#include <nx/network/cloud/tunnel/udp/acceptor.h>
#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/udp_server.h>
#include <nx/network/test_support/stun_async_client_mock.h>
#include <nx/utils/test_support/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {
namespace test {

const String kRemotePeerId("SomePeerId");
const String kConnectionSessionId("SomeSessionId");
const std::chrono::milliseconds kSocketTimeout(2000);
const std::chrono::milliseconds kUdpRetryTimeout(500);
const size_t kPleaseStopRunCount(10);

class DummyCloudSystemCredentialsProvider:
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
};

static DummyCloudSystemCredentialsProvider dummyCloudSystemCredentialsProvider;

class UdpHolePunchingTunnelAcceptorTest:
    public ::testing::Test
{
protected:
    UdpHolePunchingTunnelAcceptorTest():
        stunClientMock(std::make_shared<stun::test::AsyncClientMock>()),
        manualAcceptorStop(false),
        udpStunServer(&stunMessageDispatcher),
        isUdpServerEnabled(true),
        connectionRequests(0)
    {
        stunMessageDispatcher.registerRequestProcessor(
            stun::extension::methods::connectionAck,
            [this](
                std::shared_ptr<stun::AbstractServerConnection> c,
                stun::Message m)
            {
                onUdpRequest(std::move(c), std::move(m));
            });

        NX_CRITICAL(udpStunServer.bind(nx::network::SocketAddress::anyPrivateAddress));
        NX_CRITICAL(udpStunServer.listen());
        EXPECT_CALL(*stunClientMock, remoteAddress())
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Return(udpStunServer.address()));
    }

    void createAndStartAcceptor(size_t socketsToAccept)
    {
        if (mediatorConnection)
            mediatorConnection->pleaseStopSync();

        mediatorConnection = std::make_unique<hpm::api::MediatorServerTcpConnection>(
            stunClientMock, &dummyCloudSystemCredentialsProvider);

        nx::hpm::api::ConnectionParameters connectionParameters;
        connectionParameters.rendezvousConnectTimeout = kSocketTimeout;
        tunnelAcceptor.reset(new TunnelAcceptor(
            mediatorConnection->remoteAddress(),
            {get2ndPeerAddress()},
            connectionParameters));
        tunnelAcceptor->setConnectionInfo(kConnectionSessionId, kRemotePeerId);
        tunnelAcceptor->setMediatorConnection(mediatorConnection.get());
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
                std::unique_ptr<nx::network::AbstractStreamSocket> socket)
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

        nx::network::SocketAddress destinationAddress = connection->getSourceAddress();
        NX_LOGX(lm("Got connectionAck from %1")
            .arg(destinationAddress), cl_logDEBUG1);

        nx::network::SocketAddress sourceAddress = get2ndPeerAddress();
        NX_LOGX(lm("Initiate rendevous UDT connection from %1 to %2")
            .arg(sourceAddress).arg(destinationAddress), cl_logDEBUG2);

        if (connectionRequests)
            connectControlSocket(sourceAddress, destinationAddress);

        stun::Message response(stun::Header(
            stun::MessageClass::successResponse,
            stun::extension::methods::connectionAck,
            std::move(message.header.transactionId)));
        response.newAttribute<stun::extension::attrs::ResultCode>(
            hpm::api::ResultCode::ok);
        connection->sendMessage(std::move(response));
    }

    void connectControlSocket(
        const nx::network::SocketAddress& sourceAddress,
        const nx::network::SocketAddress& destinationAddress)
    {
        auto socket = std::make_unique<UdtStreamSocket>(AF_INET);
        ASSERT_TRUE(socket->setRendezvous(true));
        ASSERT_TRUE(socket->setSendTimeout(kSocketTimeout.count()));
        ASSERT_TRUE(socket->setNonBlockingMode(true));
        ASSERT_TRUE(socket->bind(sourceAddress));
        auto socketPtr = socket.get();
        socket->connectAsync(
            destinationAddress,
            [=](SystemError::ErrorCode code)
            {
                NX_LOGX(lm("Rendevous UDT connection from %1 to %2: %3")
                    .arg(sourceAddress).arg(destinationAddress)
                    .arg(SystemError::toString(code)), cl_logDEBUG1);

                ASSERT_EQ(code, SystemError::noError);
                selectControlSocket(socketPtr, destinationAddress);
            });

        connectSockets.push_back(std::move(socket));
    }

    void selectControlSocket(
        UdtStreamSocket* socket,
        const nx::network::SocketAddress& destinationAddress)
    {
        stun::Message request(stun::Header(
            stun::MessageClass::request,
            stun::extension::methods::tunnelConnectionChosen));

        auto buffer = std::make_shared<Buffer>();
        buffer->reserve(1000);
        stun::MessageSerializer serializer;
        size_t processed;
        serializer.setMessage(&request);
        serializer.serialize(buffer.get(), &processed);

        socket->sendAsync(
            *buffer,
            [=](SystemError::ErrorCode code, size_t size)
        {
            ASSERT_EQ(code, SystemError::noError);
            ASSERT_EQ((size_t)buffer->size(), size);
            buffer->resize(0);
            socket->readSomeAsync(
                buffer.get(),
                [=](SystemError::ErrorCode code, size_t /*size*/)
                {
                    ASSERT_EQ(code, SystemError::noError);

                    stun::Message response;
                    stun::MessageParser parser;
                    parser.setMessage(&response);

                    size_t processed;
                    ASSERT_EQ(parser.parse(*buffer, &processed),
                              nx::network::server::ParserState::done);
                    ASSERT_EQ(response.header.messageClass,
                              stun::MessageClass::successResponse);
                    ASSERT_EQ(response.header.method,
                              stun::extension::methods::tunnelConnectionChosen);

                    connectClientSocket(destinationAddress);
                });
        });
    }

    void connectClientSocket(const nx::network::SocketAddress& address)
    {
        auto socket = std::make_unique<UdtStreamSocket>(AF_INET);
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

    nx::network::SocketAddress get2ndPeerAddress()
    {
        if (!m_udtAddressKeeper)
        {
            m_udtAddressKeeper.reset(new UdtStreamSocket(AF_INET));
            EXPECT_TRUE(m_udtAddressKeeper->bind(nx::network::SocketAddress("127.0.0.1:0")));
        }

        return m_udtAddressKeeper->getLocalAddress();
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

    std::shared_ptr<stun::test::AsyncClientMock> stunClientMock;
    std::unique_ptr<hpm::api::MediatorServerTcpConnection> mediatorConnection;

    bool manualAcceptorStop;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_udtAddressKeeper;
    std::unique_ptr<TunnelAcceptor> tunnelAcceptor;
    stun::MessageDispatcher stunMessageDispatcher;
    stun::UdpServer udpStunServer;

    bool isUdpServerEnabled;
    size_t connectionRequests;
    utils::TestSyncQueue<SystemError::ErrorCode> connectResults;
    std::vector<std::unique_ptr<nx::network::AbstractStreamSocket>> connectSockets;

    utils::TestSyncQueue<SystemError::ErrorCode> acceptResults;
    std::unique_ptr<AbstractIncomingTunnelConnection> tunnelConnection;
    std::vector<std::unique_ptr<nx::network::AbstractStreamSocket>> acceptedSockets;
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
    manualAcceptorStop = true;
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int64_t> distribution(0, kUdpRetryTimeout.count() * 2);
    for (size_t i = 0; i < kPleaseStopRunCount; ++i)
    {
        const auto delay = distribution(generator);
        NX_LOG(lm("== %1 == delay: %2 ms").arg(i).arg(delay), cl_logDEBUG2);

        createAndStartAcceptor(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        tunnelAcceptor->pleaseStopSync();
        tunnelAcceptor.reset();

        if (!acceptResults.isEmpty())
        {
            ASSERT_NE(acceptResults.pop(), SystemError::noError);
        }
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
