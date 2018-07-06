#include <gtest/gtest.h>

#include <nx/utils/std/future.h>
#include <nx/network/aio/timer.h>

#include <nx/utils/scope_guard.h>

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
                m_timedoutPromise.set_value(true);
            });
    }

    void assertIfTimerHasNotShot()
    {
        ASSERT_TRUE(m_timedoutPromise.get_future().get());
    }

private:
    nx::utils::promise<bool> m_timedoutPromise;
};

TEST_F(AioTimer, handler_gets_called)
{
    startTimerFor(std::chrono::milliseconds(12));
    assertIfTimerHasNotShot();
}

TEST_F(AioTimer, zero_timeout)
{
    startTimerFor(std::chrono::milliseconds::zero());
    assertIfTimerHasNotShot();
}

TEST_F(AioTimer, destroying_in_timeout_handler)
{
    startTimerFor(
        std::chrono::milliseconds::zero(),
        [this](){ m_timer.reset(); });
    assertIfTimerHasNotShot();
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

class FtAioTimer:
    public AioTimer
{
};

TEST_F(FtAioTimer, modify_timeout)
{
    for (int i = 0; i < 29; ++i)
    {
        aio::Timer timer;
        auto timerGuard = makeScopeGuard([&timer]() { timer.pleaseStopSync(); });

        nx::utils::promise<void> timedoutPromise;
        auto timeoutHandler = [&timedoutPromise]{ timedoutPromise.set_value(); };
        timer.start(std::chrono::seconds(250), timeoutHandler);
        timer.start(std::chrono::milliseconds(250), timeoutHandler);

        ASSERT_EQ(
            std::future_status::ready,
            timedoutPromise.get_future().wait_for(std::chrono::seconds(3)));
    }
}

TEST_F(FtAioTimer, cancellation)
{
    const auto timeout = std::chrono::seconds(2);

    nx::utils::promise<void> timedoutPromise;
    auto timeoutHandler = [&timedoutPromise] { timedoutPromise.set_value(); };

    aio::Timer timer;
    auto timerGuard = makeScopeGuard([&timer]() { timer.pleaseStopSync(); });

    timer.start(timeout, timeoutHandler);
    timer.post([&timer]{ timer.cancelSync(); });

    ASSERT_EQ(
        std::future_status::timeout,
        timedoutPromise.get_future().wait_for(timeout*2));
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
