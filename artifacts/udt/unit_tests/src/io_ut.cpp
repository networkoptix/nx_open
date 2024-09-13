#include "test_fixture.h"

#include <thread>

#include <nx/network/socket_global.h>
#include <nx/network/system_socket_address.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/system_error.h>

#include "udp_proxy.h"

namespace udt::test {

template<typename Config>
class Io:
    public BasicFixture
{
    using base_type = BasicFixture;

public:

    constexpr const auto& GetParamType() { return typeid(Config); };

    Io()
    {
        initializeUdt();
    }

    ~Io()
    {
        if (m_recvThread.joinable())
            m_recvThread.join();

        deinitializeUdt();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        Config::SetUp(this);

        givenListeningServerSocket();
    }

    virtual void TearDown() override
    {
        base_type::TearDown();

        closeClientSocket();
        closeServerSocket();
    }

    void givenSocketBlockedInRecv()
    {
        givenConnectedClientSocket();

        m_recvThread = std::thread(
            [this]()
            {
                m_recvResultQueue.push(readClientSocketSync());
            });
    }

    void givenThreadThatIssuesMultipleRecv()
    {
        givenConnectedClientSocket();
        setRecvTimeout(std::chrono::milliseconds(1));

        m_recvThread = std::thread(
            [this]()
            {
                for (int i = 0; i < 51; ++i)
                    m_recvResultQueue.push(readClientSocketSync());
            });
    }

    void whenCloseSocket()
    {
        closeClientSocket();
    }

    void setNonBlockingMode(UDTSOCKET socket, bool mode)
    {
        bool value = !mode;
        ASSERT_EQ(0, UDT::setsockopt(socket, 0, UDT_SNDSYN, &value, sizeof(value)));
        ASSERT_EQ(0, UDT::setsockopt(socket, 0, UDT_RCVSYN, &value, sizeof(value)));
    }

    void whenSendData()
    {
        m_data.resize(32);
        std::generate(
            m_data.begin(), m_data.end(),
            []() { return (char)(rand() % ('z' - 'A') + 'A'); });

        whenClientSends(m_data);
    }

    void thenRecvCompleted()
    {
        m_recvResultQueue.pop();
    }

    void thenDataIsReceivedOnTheOtherSide()
    {
        thenServerReceives(m_data);
    }

private:
    std::thread m_recvThread;
    nx::utils::SyncQueue<int> m_recvResultQueue;
    std::string m_data;
};

TYPED_TEST_SUITE_P(Io);


//-------------------------------------------------------------------------------------------------

struct CommonConfig
{
    static void SetUp(BasicFixture*) {}
};

struct ReorderedPacketsConfig
{
    static void SetUp(BasicFixture* basicFixture) { basicFixture->installReorderingChannel(); }
};

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(Io, ping)
{
    this->givenTwoConnectedSockets();
    this->whenSendData();
    this->thenDataIsReceivedOnTheOtherSide();
}

TYPED_TEST_P(Io, closing_socket_while_reading_in_another_thread_is_supported)
{
    this->givenThreadThatIssuesMultipleRecv();
    this->whenCloseSocket();
    //this->thenRecvCompleted();
}

TYPED_TEST_P(Io, shutdown_interrupts_recv)
{
    this->givenSocketBlockedInRecv();
    this->whenShutdownClientSocket();
    this->thenRecvCompleted();
}

TYPED_TEST_P(Io, the_data_is_received_after_sending_socket_closure)
{
    this->givenTwoConnectedSockets();

    this->whenSendData();
    this->closeClientSocket();

    this->thenDataIsReceivedOnTheOtherSide();
}

TYPED_TEST_P(Io, the_data_is_received_after_sending_socket_closure_async)
{
    if (this->GetParamType() == typeid(ReorderedPacketsConfig))
    {
        /* Not supported.
         * UDT does't reorder 'close' control packet, also this test emulate reordered packet inaccurate
         * It is not same real network: it just doesn't send data packet after closing sender socket.
         */

        return;
    }

    this->givenTwoConnectedSockets();

    this->setNonBlockingMode(this->clientSocket(), true);
    this->whenSendData();
    this->closeClientSocket();

    this->thenDataIsReceivedOnTheOtherSide();
}

REGISTER_TYPED_TEST_SUITE_P(Io,
    ping,
    closing_socket_while_reading_in_another_thread_is_supported,
    shutdown_interrupts_recv,
    the_data_is_received_after_sending_socket_closure,
    the_data_is_received_after_sending_socket_closure_async
);

//-------------------------------------------------------------------------------------------------

INSTANTIATE_TYPED_TEST_SUITE_P(Common, Io, CommonConfig);
INSTANTIATE_TYPED_TEST_SUITE_P(ReorderedPackets, Io, ReorderedPacketsConfig);

//-------------------------------------------------------------------------------------------------

class Connect:
    public Io<CommonConfig>
{
    using base_type = Io;

public:
    ~Connect()
    {
        if (m_rawUdpReceiverThread.joinable())
            m_rawUdpReceiverThread.join();

        if (m_connectThread.joinable())
            m_connectThread.join();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenClientSocket();
    }

    virtual void TearDown() override
    {
        UDT::shutdown(clientSocket(), UDT_SHUT_RDWR);

        base_type::TearDown();
    }

    void startRawUdpReceiver()
    {
        m_udpServer = socket(ipVersion(), SOCK_DGRAM, IPPROTO_UDP);
        ASSERT_GT(m_udpServer, 0);

        detail::SocketAddress localAddress;
        if (ipVersion() == AF_INET)
        {
            localAddress.setFamily(ipVersion());
            inet_pton(AF_INET, "127.0.0.1", &(localAddress.v4().sin_addr.s_addr));
        }
        else
        {
            localAddress.setFamily(ipVersion());
            localAddress.v6().sin6_addr = in6addr_loopback;
        }

        ASSERT_EQ(0, bind(m_udpServer, localAddress.get(), localAddress.size()));
        ASSERT_EQ(0, getsockname(
            m_udpServer,
            m_rawUdpReceiverAddress.get(), &m_rawUdpReceiverAddress.length()));

        m_rawUdpReceiverThread =
            std::thread([this]() { rawUdpReceiverFunc(); });
    }

    void setSendTimeout(std::chrono::milliseconds timeout)
    {
        int timeoutInt = timeout.count();
        ASSERT_EQ(0, UDT::setsockopt(
            clientSocket(), 0, UDT_SNDTIMEO, &timeoutInt, sizeof(timeoutInt)));
    }

    void whenConnect()
    {
        const auto result = UDT::connect(
            clientSocket(), m_rawUdpReceiverAddress.get(), m_rawUdpReceiverAddress.size());
        m_lastResultCode = result == 0 ? SystemError::noError : UDT::getlasterror().osError();
    }

    void whenConnectFromADifferentThread()
    {
        m_connectThread = std::thread([this]() { whenConnect(); });
    }

    void thenConnectFailedWithTimeout()
    {
        ASSERT_EQ(SystemError::timedOut, *m_lastResultCode);
    }

    void thenConnectIsStillRunning()
    {
        ASSERT_FALSE(m_lastResultCode);
    }

private:
    std::thread m_rawUdpReceiverThread;
    std::thread m_connectThread;
    int m_udpServer = -1;
    detail::SocketAddress m_rawUdpReceiverAddress;
    std::optional<SystemError::ErrorCode> m_lastResultCode;

    void rawUdpReceiverFunc()
    {
        // TODO
    }
};

TEST_F(Connect, timeout)
{
    startRawUdpReceiver();

    setSendTimeout(std::chrono::milliseconds(1));
    whenConnect();
    thenConnectFailedWithTimeout();
}

TEST_F(Connect, no_timeout_by_default)
{
    startRawUdpReceiver();

    whenConnectFromADifferentThread();

    // NOTE: The default timeout was 3 seconds.
    std::this_thread::sleep_for(std::chrono::seconds(5));

    thenConnectIsStillRunning();
}

//-------------------------------------------------------------------------------------------------

class IoThroughMalfunctingChannel:
    public BasicFixture
{
    using base_type = BasicFixture;

public:
    IoThroughMalfunctingChannel()
    {
    }

    ~IoThroughMalfunctingChannel()
    {
        m_stopped = true;
        for (auto& t: m_ioThreads)
            t.join();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenListeningServerSocket();

        m_serverAddress = nx::network::SystemSocketAddress(AF_INET);
        ASSERT_EQ(0, UDT::getsockname(serverSocket(), m_serverAddress.get(),
            reinterpret_cast<int*>(&m_serverAddress.length())));

        ASSERT_TRUE(m_proxy.start());
        m_proxy.setBeforeSendingPacketCallback([this](auto&&... args) {
            return alterUdtTraffic(std::forward<decltype(args)>(args)...);
        });
    }

    void givenClientSocketConnectedThroughProxy()
    {
        givenClientSocket();

        const nx::network::SystemSocketAddress anyAddr(
            nx::network::SocketAddress::anyPrivateAddressV4, AF_INET);

        ASSERT_EQ(0, UDT::bind(clientSocket(), anyAddr.get(), anyAddr.length()));

        nx::network::SystemSocketAddress addr(AF_INET);
        ASSERT_EQ(0, UDT::getsockname(clientSocket(), addr.get(), reinterpret_cast<int*>(&addr.length())));

        m_proxy.addProxyRoute(addr.toSocketAddress(), m_serverAddress.toSocketAddress());
        m_proxy.addProxyRoute(m_serverAddress.toSocketAddress(), addr.toSocketAddress());

        nx::network::SystemSocketAddress proxyAddr(m_proxy.getProxyAddress(), AF_INET);
        ASSERT_EQ(0, UDT::connect(clientSocket(), proxyAddr.get(), proxyAddr.length()));
        m_serverConnection = whenAcceptConnection();
        ASSERT_NE(UDT::INVALID_SOCK, m_serverConnection);
    }

    void setPacketCorruptionRate(double rate)
    {
        m_corruptionRate = rate;
    }

    void setPacketLossRate(double rate)
    {
        m_packetLossRate = rate;
    }

    void startTwoWayDataExchange()
    {
        int timeoutMs = 500;

        UDT::setsockopt(m_serverConnection, 0, UDT_RCVTIMEO, &timeoutMs, sizeof(timeoutMs));
        UDT::setsockopt(clientSocket(), 0, UDT_RCVTIMEO, &timeoutMs, sizeof(timeoutMs));

        m_ioThreads.push_back(std::thread([this]() {
            dataExchangeThreadMain(m_serverConnection, clientSocket());
        }));

        m_ioThreads.push_back(std::thread([this]() {
            dataExchangeThreadMain(clientSocket(), m_serverConnection);
        }));
    }

private:
    bool alterUdtTraffic(char* buf, std::size_t len)
    {
        if ((rand() % 100) < 100 * m_packetLossRate)
            return false;

        if ((rand() % 100) < 100 * m_corruptionRate)
        {
            const int corruptedBytesCnt = (rand() % 7) + 1;
            int pos = 0;
            for (int i = 0; i < corruptedBytesCnt; ++i)
            {
                pos = (pos + (rand() % len)) % len;
                buf[pos] ^= buf[pos];
            }
        }

        return true;
    }

    void dataExchangeThreadMain(UDTSOCKET from, UDTSOCKET to)
    {
        char buf[4096];
        while (!m_stopped)
        {
            UDT::send(to, buf, sizeof(buf) / 2, 0);
            int cnt = UDT::recv(from, buf, sizeof(buf), 0);
            if (cnt > 0)
                m_totalBytesTransferred += cnt;
        }
    }

private:
    nx::network::SystemSocketAddress m_serverAddress;
    nx::network::SocketGlobalsHolder m_socketInitializer;
    UdpProxy m_proxy;
    UDTSOCKET m_serverConnection = UDT::INVALID_SOCK;
    bool m_stopped = false;
    std::vector<std::thread> m_ioThreads;
    std::atomic<int64_t> m_totalBytesTransferred = 0;
    double m_corruptionRate = 0.0;
    double m_packetLossRate = 0.0;
};

TEST_F(IoThroughMalfunctingChannel, corrupted_packets_do_not_crash_the_process)
{
    givenClientSocketConnectedThroughProxy();

    setPacketCorruptionRate(0.3);
    setPacketLossRate(0.1);

    startTwoWayDataExchange();

    std::this_thread::sleep_for(std::chrono::seconds(7));
    // the process does not crash.
}

} // namespace udt::test
