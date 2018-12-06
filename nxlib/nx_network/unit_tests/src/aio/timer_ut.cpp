#include <gtest/gtest.h>

#include <nx/network/aio/timer.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class AioTimer:
    public ::testing::Test
{
public:
    AioTimer()
    {
        m_timer = std::make_unique<aio::Timer>();
    }

    ~AioTimer()
    {
        if (m_timer)
        {
            m_timer->pleaseStopSync();
            m_timer.reset();
        }
    }

protected:
    std::unique_ptr<aio::Timer> m_timer;

    void startTimerFor(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void()> customHandler = nullptr)
    {
        m_timer->start(
            timeout,
            [this, customHandler = std::move(customHandler)]
            {
                if (customHandler)
                    customHandler();
                m_timerEvents.push(/*dummy*/ 0);
            });
    }

    void cancelTimer()
    {
        m_timer->cancelSync();
    }

    void assertTimerEventRaised()
    {
        m_timerEvents.pop();
    }

    void assertTimerEventNotRaised(std::chrono::milliseconds timeToWait)
    {
        ASSERT_FALSE(m_timerEvents.pop(timeToWait));
    }

private:
    nx::utils::SyncQueue<int /*dummy*/> m_timerEvents;
};

TEST_F(AioTimer, handler_gets_called)
{
    startTimerFor(std::chrono::milliseconds(12));
    assertTimerEventRaised();
}

TEST_F(AioTimer, zero_timeout)
{
    startTimerFor(std::chrono::milliseconds::zero());
    assertTimerEventRaised();
}

TEST_F(AioTimer, destroying_in_timeout_handler)
{
    startTimerFor(
        std::chrono::milliseconds::zero(),
        [this](){ m_timer.reset(); });
    assertTimerEventRaised();
}

TEST_F(AioTimer, cancel_timer_does_not_cancel_posted_calls)
{
    nx::utils::promise<void> postInvokedPromise;

    m_timer->start(
        std::chrono::seconds::zero(),
        [this, &postInvokedPromise]()
        {
            m_timer->post([&postInvokedPromise]() { postInvokedPromise.set_value(); });
            m_timer->cancelSync();
        });
    postInvokedPromise.get_future().wait();
}

TEST_F(AioTimer, modify_timeout)
{
    startTimerFor(std::chrono::hours(24));
    startTimerFor(std::chrono::milliseconds(1));

    assertTimerEventRaised();
}

TEST_F(AioTimer, modified_timer_is_invoked_only_once)
{
    const auto timeout = std::chrono::milliseconds(1);

    m_timer->executeInAioThreadSync(
        [this, timeout]()
        {
            startTimerFor(timeout);
            startTimerFor(timeout*2);
        });

    assertTimerEventRaised();

    assertTimerEventNotRaised(timeout*10);
}

TEST_F(AioTimer, cancelled_timer_is_not_raised)
{
    const auto timeout = std::chrono::milliseconds(1);

    m_timer->executeInAioThreadSync(
        [this, timeout]()
        {
            startTimerFor(timeout);
            cancelTimer();
        });

    assertTimerEventNotRaised(timeout*10);
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
