#include <gtest/gtest.h>

#include <nx/network/aio/repetitive_timer.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::aio::test {

class RepetitiveTimer:
    public ::testing::Test
{
public:
    RepetitiveTimer():
        m_timeout(std::chrono::milliseconds(1))
    {
    }

    ~RepetitiveTimer()
    {
        m_timer.pleaseStopSync();
    }

protected:
    void registerEventHandler(TimerEventHandler timerEventHandler)
    {
        m_timerEventHandler = std::move(timerEventHandler);
    }

    void startTimer()
    {
        startTimer(m_timeout);
    }

    void startTimer(std::chrono::milliseconds timeout)
    {
        m_timer.start(
            timeout,
            [this]()
            {
                m_timerEvents.push(++m_counter);
                if (m_timerEventHandler)
                    m_timerEventHandler();
            });
    }

    void cancelTimer()
    {
        m_timer.cancelSync();
    }

    void waitForOneEvent()
    {
        m_timerEvents.pop();
    }
    
    void waitForMultipleEvents()
    {
        for (int i = 0; i < 3; ++i)
            m_timerEvents.pop();
    }

    void assertNoMoreEventsAreReported()
    {
        ASSERT_FALSE(m_timerEvents.pop(m_timeout * 7));
    }

    std::chrono::milliseconds timeout() const
    {
        return m_timeout;
    }

private:
    aio::RepetitiveTimer m_timer;
    int m_counter = 0;
    const std::chrono::milliseconds m_timeout;
    nx::utils::SyncQueue<int> m_timerEvents;
    TimerEventHandler m_timerEventHandler;
};

TEST_F(RepetitiveTimer, invokes_the_handler_repeatedly)
{
    startTimer();
    waitForMultipleEvents();
}

TEST_F(RepetitiveTimer, can_be_cancelled_in_event_handler)
{
    registerEventHandler([this]() { cancelTimer(); });

    startTimer();
    waitForOneEvent();

    assertNoMoreEventsAreReported();
}

TEST_F(RepetitiveTimer, timeout_can_be_changed)
{
    registerEventHandler(
        [this]()
        {
            cancelTimer();
            startTimer(timeout() + std::chrono::milliseconds(1));
        });

    startTimer();
    waitForMultipleEvents();
}

} // namespace nx::network::aio::test
