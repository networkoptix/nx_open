#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_thread.h>
#include <nx/network/aio/aio_event_handler.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class AIOThread:
    public ::testing::Test,
    public aio::AIOEventHandler
{
public:
    AIOThread()
    {
        m_tcpSocket = std::make_unique<TCPSocket>(AF_INET);
    }

protected:
    void givenSocketBeingPolled()
    {
        whenStartPollingSocket();
    }

    void whenStartPollingSocket()
    {
        m_aioThread.startMonitoring(m_tcpSocket.get(), aio::EventType::etRead, this);
    }

    void whenStopPollingSocket()
    {
        m_aioThread.stopMonitoring(m_tcpSocket.get(), aio::EventType::etRead, false);
    }

private:
    std::unique_ptr<TCPSocket> m_tcpSocket;
    aio::AIOThread m_aioThread;

    virtual void eventTriggered(Pollable* /*sock*/, aio::EventType /*eventType*/) throw() override
    {
    }
};

TEST_F(AIOThread, unexpected_stop_polling)
{
    whenStopPollingSocket();
    //thenProcessHasNotCrashed();
}

TEST_F(AIOThread, duplicate_start_polling)
{
    givenSocketBeingPolled();

    whenStartPollingSocket();
    whenStopPollingSocket();

    //thenProcessHasNotCrashed();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
