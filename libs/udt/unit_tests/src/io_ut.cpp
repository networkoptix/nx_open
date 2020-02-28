#include "test_fixture.h"

#include <algorithm>
#include <mutex>
#include <thread>

#include <nx/utils/thread/sync_queue.h>

#include <udt/channel.h>

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

TEST_F(Io, the_data_is_received_after_sending_socket_closure)
{
    givenTwoConnectedSockets();

    whenSendData();
    closeClientSocket();

    thenDataIsReceivedOnTheOtherSide();
}

//-------------------------------------------------------------------------------------------------

class ReorderingChannel:
    public UdpChannel
{
    using base_type = UdpChannel;

public:
    ReorderingChannel::ReorderingChannel(int ipVersion):
        base_type(ipVersion)
    {
        m_sendThread = std::thread([this]() { sendThreadMain(); });
    }

    ~ReorderingChannel()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_terminated = true;
            m_cond.notify_all();
        }

        m_sendThread.join();
    }

    virtual Result<int> sendto(const detail::SocketAddress& addr, CPacket packet) override
    {
        static constexpr auto kMaxDelay = std::chrono::milliseconds(999);

        std::lock_guard<std::mutex> lock(m_mutex);

        if (packet.getFlag() == PacketFlag::Control)
            return base_type::sendto(addr, std::move(packet));

        // Delaying every data packet to force FIN before the last data.

        const auto [ioBufs, count] = packet.ioBufs();
        const auto packetSize = std::accumulate(
            ioBufs, ioBufs + count, 0,
            [](auto one, auto two) { return one + two.len; });

        m_sendTasks.emplace(
            std::chrono::steady_clock::now() + kMaxDelay,
            SendTask{addr, std::move(packet)});

        m_cond.notify_all();

        return packetSize;
    }

private:
    struct SendTask
    {
        detail::SocketAddress addr;
        CPacket packet;
    };

    std::mutex m_mutex;
    std::thread m_sendThread;
    bool m_terminated = false;
    std::map<std::chrono::steady_clock::time_point /*deadline*/, SendTask> m_sendTasks;
    std::condition_variable m_cond;

    void sendThreadMain()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (!m_terminated)
        {
            const auto curTime = std::chrono::steady_clock::now();

            if (!m_sendTasks.empty() && m_sendTasks.begin()->first <= curTime)
            {
                send(std::move(m_sendTasks.begin()->second));
                m_sendTasks.erase(m_sendTasks.begin());
            }
            else if (m_sendTasks.empty())
            {
                m_cond.wait(lock);
            }
            else if (m_sendTasks.begin()->first > curTime)
            {
                m_cond.wait_for(lock, m_sendTasks.begin()->first - curTime);
            }
        }
    }

    void send(SendTask sendTask)
    {
        base_type::sendto(sendTask.addr, std::move(sendTask.packet));
    }
};

//-------------------------------------------------------------------------------------------------

class IoOverReorderingChannel:
    public Io
{
    using base_type = Io;

public:
    ~IoOverReorderingChannel()
    {
        if (m_channelFactoryBak)
        {
            UdpChannelFactory::instance().setCustomFunc(std::move(*m_channelFactoryBak));
            m_channelFactoryBak = std::nullopt;
        }
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        installReorderingChannel();
    }

private:
    std::optional<UdpChannelFactory::FactoryFunc> m_channelFactoryBak;

    void installReorderingChannel()
    {
        m_channelFactoryBak = UdpChannelFactory::instance().setCustomFunc(
            [this](int ipVersion) { return std::make_unique<ReorderingChannel>(ipVersion); });
    }
};

// TODO: #ak Remove copy-paste. Make test template.
TEST_F(IoOverReorderingChannel, the_data_is_received_after_sending_socket_closure)
{
    givenTwoConnectedSockets();

    whenSendData();
    closeClientSocket();

    thenDataIsReceivedOnTheOtherSide();
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
