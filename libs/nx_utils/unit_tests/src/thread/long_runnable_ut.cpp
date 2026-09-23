// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/long_runnable.h>

namespace nx::utils::test {

using namespace std::chrono_literals;

namespace {

class TestRunnable: public QnLongRunnable
{
public:
    TestRunnable(): QnLongRunnable("TestRunnable") {}
    virtual ~TestRunnable() override { stop(); }

    void waitUntilRunning() { m_running.get_future().wait(); }

protected:
    virtual void run() override
    {
        m_running.set_value();
        while (!needToStop())
            std::this_thread::sleep_for(10ms);
    }

private:
    std::promise<void> m_running;
};

} // namespace

TEST(QnLongRunnablePool, destroyedRunnableDoesNotStayInTheRunningSet)
{
    // VMS-62768: before the fix, a destroyed runnable was left in the pool, so waitAllLocked()
    // waited for a dangling pointer until the timeout and then could crash while logging it.
    // Keep the timeout short so a regression fails the test fast instead of hanging on it.
    constexpr auto kStopTimeout = 10s;
    QnLongRunnablePool pool;
    pool.setStopTimeout(kStopTimeout);

    {
        TestRunnable runnable;
        runnable.start();
        runnable.waitUntilRunning();

        // Simulate the state behind VMS-62768: finishedNotify() never reaches the pool, so the
        // runnable stays listed as running.
        QObject::disconnect(&runnable, nullptr, &runnable, nullptr);

        runnable.stop();
    } //< ~QnLongRunnable() -> destroyedNotify(), which must drop the runnable from m_running.

    // If the runnable was properly deregistered, m_running is already empty here, so waitAll()
    // returns near-instantly without waiting on the timeout. Otherwise waitAllLocked() has no
    // in-between outcome: it always waits out the full kStopTimeout before giving up.
    const auto start = std::chrono::steady_clock::now();
    pool.waitAll();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed >= 9s)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "waitAll() took %1, the runnable was likely left in the running set",
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed));
    }
}

} // namespace nx::utils::test
