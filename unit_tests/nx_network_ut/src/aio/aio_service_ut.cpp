#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/synchronous_tcp_server.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

namespace {

class TestTcpServer:
    public network::test::SynchronousTcpServer
{
protected:
    virtual void processConnection(AbstractStreamSocket* connection) override
    {
        ASSERT_TRUE(connection->setRecvTimeout(0));

        nx::Buffer buf;
        buf.resize(100);
        connection->recv(buf.data(), buf.size());
    }
};

} // namespace

class AIOService:
    public ::testing::Test,
    public AIOEventHandler
{
public:
    ~AIOService()
    {
        m_service.stopMonitoring(m_socket.get(), aio::EventType::etRead, true);
        m_service.stopMonitoring(m_socket.get(), aio::EventType::etTimedOut, true);

        m_service.pleaseStopSync();
    }

protected:
    void givenSocket()
    {
        m_socket = std::make_unique<TCPSocket>(AF_INET);
    }

    void givenConnectedSocket()
    {
        givenSocket();

        ASSERT_TRUE(m_socket->connect(m_tcpServer.endpoint(), nx::network::kNoTimeout)) <<
            SystemError::getLastOSErrorText().toStdString();
    }

    void givenSocketBeingMonitored()
    {
        givenConnectedSocket();
        whenSocketMonitoringStarted();
    }

    void whenSocketMonitoringStarted()
    {
        m_service.startMonitoring(m_socket.get(), aio::EventType::etRead, this);
        m_monitoredEvent = aio::EventType::etRead;
    }

    void whenRandomlyAddedAndRemovedSocketMultipleTimes()
    {
        static const auto eventType = EventType::etTimedOut;
        static const int stepCount = 100;

        std::vector<int> steps;
        steps.reserve(stepCount);

        for (int i = 0; i < stepCount; ++i)
        {
            const int action = rand() % 2;
            steps.push_back(action);
            if (action == 0)
            {
                if (eventType == EventType::etTimedOut)
                    m_service.registerTimer(m_socket.get(), std::chrono::milliseconds(1), this);
                else
                    m_service.startMonitoring(m_socket.get(), eventType, this);
            }
            else if (action == 1)
            {
                m_service.post(
                    m_socket.get(),
                    [this]()
                    {
                        m_service.stopMonitoring(m_socket.get(), eventType, true);
                    });
            }
            else if (action == 2)
            {
                m_service.stopMonitoring(m_socket.get(), eventType, rand() & 1);
            }
        }
    }

    void whenSocketMonitoringTimeoutIsChanged()
    {
        ASSERT_TRUE(m_socket->setRecvTimeout(100))
            << SystemError::getLastOSErrorText().toStdString();

        whenSocketMonitoringStarted();
    }

    void whenEventIsRaised()
    {
        m_tcpServer.waitForAtLeastOneConnection();
        m_tcpServer.anyConnection()->send(nx::utils::random::generate(16));
    }

    void thenEventHasBeenReported()
    {
        auto event = m_eventsReported.pop();
        ASSERT_EQ(m_socket.get(), event.sock);
        ASSERT_EQ(m_monitoredEvent, event.eventType);
    }

    void thenTimedoutHasBeenReported()
    {
        auto event = m_eventsReported.pop();
        ASSERT_EQ(m_socket.get(), event.sock);
        ASSERT_EQ(aio::EventType::etReadTimedOut, event.eventType);
    }

private:
    struct MonitoringEvent
    {
        Pollable* sock = nullptr;
        aio::EventType eventType = aio::EventType::etRead;
    };

    aio::AIOService m_service;
    TestTcpServer m_tcpServer;
    std::unique_ptr<TCPSocket> m_socket;
    nx::utils::SyncQueue<MonitoringEvent> m_eventsReported;
    aio::EventType m_monitoredEvent = aio::EventType::etNone;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_service.initialize(1));

        ASSERT_TRUE(m_tcpServer.bindAndListen(SocketAddress::anyPrivateAddress));
        m_tcpServer.start();
    }

    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) throw() override
    {
        MonitoringEvent event;
        event.sock = sock;
        event.eventType = eventType;
        m_eventsReported.push(event);
    }
};

TEST_F(AIOService, socket_correctly_added_and_removed_multiple_times)
{
    givenSocket();
    whenRandomlyAddedAndRemovedSocketMultipleTimes();
    //thenProcessHasNotCrashed();
}

TEST_F(AIOService, changing_socket_operation_timeout)
{
    givenSocketBeingMonitored();
    whenSocketMonitoringTimeoutIsChanged();
    thenTimedoutHasBeenReported();
}

TEST_F(AIOService, duplicate_start_monitoring_call_is_ignored)
{
    givenSocketBeingMonitored();

    whenSocketMonitoringStarted();
    whenEventIsRaised();

    thenEventHasBeenReported();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
