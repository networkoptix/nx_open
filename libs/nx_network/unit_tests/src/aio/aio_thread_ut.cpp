// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <future>
#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/pollset_wrapper.h>
#include <nx/network/aio/pollset.h>
#include <nx/network/aio/aio_thread.h>
#include <nx/network/aio/aio_event_handler.h>
#include <nx/network/aio/aio_task_queue.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::aio::test {

class DummyEventHandler:
    public AIOEventHandler
{
public:
    DummyEventHandler(std::function<void(Pollable*, aio::EventType)> func):
        m_func(std::move(func))
    {
    }

    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) throw() override
    {
        m_func(sock, eventType);
    }

private:
    std::function<void(Pollable*, aio::EventType)> m_func;
};

//-------------------------------------------------------------------------------------------------

class AioThread:
    public ::testing::Test,
    public aio::AIOEventHandler
{
public:
    AioThread():
        AioThread(nullptr)
    {
    }

    AioThread(std::unique_ptr<AbstractPollSet> pollSet):
        m_aioThread(std::move(pollSet)),
        m_pollable(&m_aioThread)
    {
        m_tcpSocket = std::make_unique<TCPSocket>(AF_INET);
        m_tcpSocket->bindToAioThread(&m_aioThread);
        m_aioThread.start();

        m_tcpSocket->bindToAioThread(&m_aioThread);
    }

    ~AioThread()
    {
        m_aioThread.pleaseStop();
        m_aioThread.wait();
    }

protected:
    void givenSocketBeingPolled()
    {
        whenStartPollingSocket();
    }

    void whenStartPollingSocket()
    {
        m_aioThread.startMonitoring(
            m_tcpSocket.get(),
            aio::EventType::etRead,
            this,
            std::nullopt,
            nullptr);
    }

    void whenStopPollingSocket()
    {
        m_aioThread.stopMonitoring(m_tcpSocket.get(), aio::EventType::etRead);
    }

    void whenReadingSocketWithTimeout()
    {
        std::promise<void> added;

        m_aioThread.startMonitoring(
            m_tcpSocket.get(),
            aio::EventType::etRead,
            this,
            std::chrono::milliseconds(std::chrono::hours(1)),
            [&added]() { added.set_value(); });

        added.get_future().wait();
    }

    void whenRegisterSocketTimer()
    {
        m_tcpSocket->registerTimer(std::chrono::hours(1), nullptr);
    }

    void whenDeleteSocket()
    {
        m_pollable.executeInAioThreadSync([this]() { m_tcpSocket.reset(); });
    }

    void thenTimerTaskIsRemovedFromAio()
    {
        ASSERT_EQ(0U, m_aioThread.taskQueue().periodicTasksCount());
    }

    aio::AioThread m_aioThread;
    std::unique_ptr<TCPSocket> m_tcpSocket;
    nx::utils::SyncQueue<aio::EventType> m_eventsReported;

private:
    BasicPollable m_pollable;

    virtual void eventTriggered(Pollable* /*sock*/, aio::EventType eventType) throw() override
    {
        m_eventsReported.push(eventType);
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(AioThread, unexpected_stop_polling)
{
    whenStopPollingSocket();
    //thenProcessHasNotCrashed();
}

TEST_F(AioThread, duplicate_start_polling)
{
    givenSocketBeingPolled();

    whenStartPollingSocket();
    whenStopPollingSocket();

    //thenProcessHasNotCrashed();
}

TEST_F(AioThread, socket_polled_notification)
{
    UDPSocket socket(AF_INET);
    std::atomic<bool> handlerCalledFlag(false);
    DummyEventHandler evHandler(
        [&handlerCalledFlag](Pollable*, aio::EventType)
        {
            handlerCalledFlag = true;
        });

    nx::utils::promise<void> started;
    m_aioThread.startMonitoring(
        &socket,
        aio::etRead,
        &evHandler,
        std::chrono::milliseconds(0),
        [&started]()
        {
            started.set_value();
        });

    started.get_future().wait();
    ASSERT_FALSE(handlerCalledFlag);

    m_aioThread.stopMonitoring(&socket, aio::etRead);
}

TEST_F(AioThread, socket_timer_task_is_removed_when_stop_monitoring_socket)
{
    whenReadingSocketWithTimeout();
    whenStopPollingSocket();

    thenTimerTaskIsRemovedFromAio();
}

TEST_F(AioThread, socket_timer_task_is_removed_when_deleting_socket)
{
    whenReadingSocketWithTimeout();
    whenDeleteSocket();

    thenTimerTaskIsRemovedFromAio();
}

TEST_F(AioThread, socket_timer_is_removed_when_deleting_socket)
{
    whenRegisterSocketTimer();
    whenDeleteSocket();

    thenTimerTaskIsRemovedFromAio();
}

//-------------------------------------------------------------------------------------------------

class FailingPollSet:
    public PollSetWrapper<aio::PollSet>
{
public:
    virtual bool add(
        Pollable* const /*sock*/,
        aio::EventType /*eventType*/,
        void* /*userData*/) override
    {
        return false;
    }
};

class AIOThreadWithFailingPollSet:
    public AioThread
{
public:
    AIOThreadWithFailingPollSet():
        AioThread(std::make_unique<FailingPollSet>())
    {
    }

protected:
    void givenSocketFailedToMonitor()
    {
        whenStartMonitoringFailed();
    }

    void whenStartMonitoringFailed()
    {
        nx::utils::promise<void> done;
        m_aioThread.startMonitoring(
            m_tcpSocket.get(),
            aio::etRead,
            this,
            std::nullopt,
            [&done]()
            {
                done.set_value();
            });
        done.get_future().wait();
    }

    void whenInvokedStopMonitoring()
    {
        nx::utils::promise<void> stopped;
        m_aioThread.post(
            m_tcpSocket.get(),
            [this, &stopped]()
            {
                m_aioThread.stopMonitoring(m_tcpSocket.get(), aio::etRead);
                stopped.set_value();
            });
        stopped.get_future().wait();
    }

    void thenSocketErrorHasBeenReported()
    {
        ASSERT_EQ(EventType::etError, m_eventsReported.pop());
    }

    void thenSocketIsInCorrectState()
    {
        for (const auto& monitoringContext: m_tcpSocket->impl()->monitoredEvents)
            ASSERT_FALSE(monitoringContext.isUsed);
    }

    void thenStopMonitoringCompletedSuccessfully()
    {
        // TODO
    }
};

TEST_F(AIOThreadWithFailingPollSet, socket_is_in_correct_state_after_start_monitoring_failure)
{
    whenStartMonitoringFailed();

    thenSocketErrorHasBeenReported();
    thenSocketIsInCorrectState();
}

TEST_F(AIOThreadWithFailingPollSet, stop_monitoring_does_nothing_after_failed_start_monitoring)
{
    givenSocketFailedToMonitor();
    whenInvokedStopMonitoring();
    thenStopMonitoringCompletedSuccessfully();
}

} // namespace nx::network::aio::test
