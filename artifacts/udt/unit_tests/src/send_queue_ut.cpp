#include <gtest/gtest.h>

#include <udt/channel.h>
#include <udt/core.h>
#include <udt/queue.h>

namespace test {

class SendQueue:
    public ::testing::Test
{
public:
    SendQueue():
        m_udpChannel(AF_INET)
    {
    }

protected:
    struct Packet
    {
        std::chrono::microseconds timestamp = std::chrono::milliseconds::zero();
    };

    virtual void SetUp() override
    {
        m_sendQueue = std::make_unique<CSndQueue>(&m_udpChannel, &m_timer);
        m_sendQueue->start();
    }

    void pushPacketToSend(Packet packet)
    {
        auto sock = std::make_shared<CUDT>();
        m_sendQueue->sndUList().update(sock);
    }

    void stopSendQueue()
    {
        m_sendQueue.reset();
    }

private:
    UdpChannel m_udpChannel;
    CTimer m_timer;
    std::unique_ptr<CSndQueue> m_sendQueue;
};

TEST_F(SendQueue, stops_immediately)
{
    pushPacketToSend({.timestamp = CTimer::getTime() + std::chrono::days(1)});

    // Waiting for the internal UDT thread to start.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    stopSendQueue();
}

} // namespace test
