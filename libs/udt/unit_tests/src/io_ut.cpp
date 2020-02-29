#include "test_fixture.h"

#include <thread>

#include <nx/utils/thread/sync_queue.h>

namespace udt::test {

template<typename Config>
class Io:
    public BasicFixture
{
    using base_type = BasicFixture;

public:
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

TYPED_TEST_CASE_P(Io);

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
    this->givenTwoConnectedSockets();

    this->setNonBlockingMode(this->clientSocket(), true);
    this->whenSendData();
    this->closeClientSocket();

    this->thenDataIsReceivedOnTheOtherSide();
}

REGISTER_TYPED_TEST_CASE_P(Io,
    ping,
    closing_socket_while_reading_in_another_thread_is_supported,
    shutdown_interrupts_recv,
    the_data_is_received_after_sending_socket_closure,
    the_data_is_received_after_sending_socket_closure_async
);

//-------------------------------------------------------------------------------------------------

struct CommonConfig
{
    static void SetUp(BasicFixture*) {}
};

INSTANTIATE_TYPED_TEST_CASE_P(Common, Io, CommonConfig);

//-------------------------------------------------------------------------------------------------

struct ReorderedPacketsConfig
{
    static void SetUp(BasicFixture* basicFixture)
    {
        basicFixture->installReorderingChannel();
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(ReorderedPackets, Io, ReorderedPacketsConfig);

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
            localAddress.v4().sin_addr.s_addr = inet_addr("127.0.0.1");
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

} // namespace udt::test
