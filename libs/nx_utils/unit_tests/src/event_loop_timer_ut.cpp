#include <nx/utils/event_loop_timer.h>

#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <QtCore/QThread>

namespace nx::utils::test {

using namespace std::chrono_literals;
using namespace std::chrono;

struct Param
{
    milliseconds checkPeriod = 0ms;
    milliseconds timeout = 0ms;
    size_t restartCount = 0;

    Param() = default;
    Param(milliseconds checkPeriod, milliseconds timeout, size_t restartCount):
        checkPeriod(checkPeriod),
        timeout(timeout),
        restartCount(restartCount)
    {
    }
};

class EventLoopTimer: public ::testing::TestWithParam<Param>
{
protected:
    virtual void SetUp() override
    {
        m_thread.start();
        m_thread.moveToThread(&m_thread);
        ASSERT_TRUE(m_thread.isRunning());
    }

    virtual void TearDown() override
    {
        m_thread.quit();
        m_thread.wait();
    }

    void whenTimerInitializedAndStarted()
    {
        m_timer.reset(new nx::utils::EventLoopTimer(GetParam().checkPeriod));
        m_timer->moveToThread(&m_thread);
        QTimer::singleShot(
            0ms,
            &m_thread,
            [this]() { m_timer->start(GetParam().timeout, [this]() { onTimer(); }); });

        std::lock_guard<std::mutex> lock(m_mutex);
        m_invocations.push_back(steady_clock::now());
    }

    void thenHandlerShouldBeInvoked()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(
            lock,
            [this]() { return m_invocations.size() == GetParam().restartCount + 2; });

        lock.unlock();
        // Waiting for some time to ensure no unexpected handler invocations have been produced.
        std::this_thread::sleep_for(
            std::min(10ms, GetParam().timeout * static_cast<int>(GetParam().restartCount + 1)));

        lock.lock();
        ASSERT_EQ(GetParam().restartCount + 2, m_invocations.size());

        for (size_t i = 0; i < m_invocations.size() - 1; ++i)
        {
            ASSERT_TRUE(
                m_invocations[i + 1] - m_invocations[i] < std::max(5ms, GetParam().timeout) * 3);
            ASSERT_TRUE(m_invocations[i + 1] - m_invocations[i] >= GetParam().timeout);
        }
    }

    void whenTimerRestarted()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_invocationsSoFar = m_invocations.size();

        QTimer::singleShot(
            0ms,
            &m_thread,
            [this]()
            {
                m_timer->start(
                    200ms,
                    [this]()
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_newHandlerInvoked = true;
                        m_newHandlerCondition.notify_one();
                    });
            });
    }

    void thenOnlyNewHandlerShouldBeInvoked()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_newHandlerCondition.wait(lock, [this]() { return m_newHandlerInvoked; });

        ASSERT_EQ(m_invocationsSoFar, m_invocations.size());
    }

private:
    QThread m_thread;
    std::unique_ptr<nx::utils::EventLoopTimer> m_timer;
    std::vector<steady_clock::time_point> m_invocations;
    size_t m_restartCount = GetParam().restartCount;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::condition_variable m_newHandlerCondition;
    size_t m_invocationsSoFar = 0;
    bool m_newHandlerInvoked = false;

    void onTimer()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_invocations.push_back(steady_clock::now());

        if (m_restartCount-- > 0)
            m_timer->start(GetParam().timeout, [this]() { onTimer(); });
        else
            m_condition.notify_one();
    }
};

TEST_P(EventLoopTimer, HandlerInvoked)
{
    whenTimerInitializedAndStarted();
    thenHandlerShouldBeInvoked();
}

TEST_P(EventLoopTimer, Restart)
{
    whenTimerInitializedAndStarted();
    whenTimerRestarted();
    thenOnlyNewHandlerShouldBeInvoked();
}

INSTANTIATE_TEST_CASE_P(EventLoopTimer_VariousTimeouts,
    EventLoopTimer,
    ::testing::Values(
        Param(2s, 10ms, 0),
        Param(2s, 10ms, 1),
        Param(2s, 10ms, 4),
        Param(10ms, 2s, 0),
        Param(10ms, 2s, 1),
        Param(10ms, 2s, 4),
        Param(1ms, 0ms, 0),
        Param(1ms, 0ms, 1),
        Param(1ms, 0ms, 4),
        Param(10ms, 0ms, 0),
        Param(10ms, 0ms, 1),
        Param(10ms, 0ms, 4),
        Param(1ms, 100ms, 0),
        Param(1ms, 100ms, 1),
        Param(1ms, 100ms, 4)
        ));

} // namespace nx::utils::test
