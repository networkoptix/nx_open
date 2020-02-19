#include "test_fixture.h"

#include <thread>

#include <nx/utils/thread/sync_queue.h>

namespace udt::test {

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

    void whenSendData()
    {
        m_data.resize(129);
        std::generate(
            m_data.begin(), m_data.end(),
            []() { return (char)(rand() % 128); });

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

TEST_F(Io, ping)
{
    givenTwoConnectedSockets();
    whenSendData();
    thenDataIsReceivedOnTheOtherSide();
}

TEST_F(Io, closing_socket_while_reading_in_another_thread_is_supported)
{
    givenThreadThatIssuesMultipleRecv();
    whenCloseSocket();
    //thenRecvCompleted();
}

TEST_F(Io, shutdown_interrupts_recv)
{
    givenSocketBlockedInRecv();
    whenShutdownClientSocket();
    thenRecvCompleted();
}

//-------------------------------------------------------------------------------------------------

class Connect:
    public Io
{
    using base_type = Io;

public:
    ~Connect()
    {
        if (m_rawUdpReceiverThread.joinable())
            m_rawUdpReceiverThread.join();

        if (m_connectThread.joinable())
        {
            UDT::shutdown(clientSocket(), UDT_SHUT_RDWR);
            m_connectThread.join();
        }
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenClientSocket();
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

TEST_F(Connect, DISABLED_no_timeout)
{
    startRawUdpReceiver();

    setSendTimeout(std::chrono::hours(1));
    whenConnectFromADifferentThread();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    thenConnectIsStillRunning();
}

} // namespace udt::test
