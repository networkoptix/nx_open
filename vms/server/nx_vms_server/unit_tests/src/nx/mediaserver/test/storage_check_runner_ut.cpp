#include <recorder/storage_check_runner.h>

#include <gtest/gtest.h>

namespace nx::vms::server::test {

using namespace std::chrono;

class StorageCheckRunnerTest: public ::testing::Test
{
protected:
    const seconds kCheckPeriod = 2s;
    const size_t kExpectedCallCount = 3;

    StorageCheckRunner runner;

    StorageCheckRunnerTest()
    {
        m_elapsedTimer.restart();
        runner.start(
            [this]()
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                m_handlerCalls.push_back(m_elapsedTimer.elapsed());
            },
            kCheckPeriod);
    }

    std::vector<milliseconds> waitForCallsAndGetResults(int count)
    {
        while (true)
        {
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                if (m_handlerCalls.size() == count)
                    return m_handlerCalls;
            }

            std::this_thread::sleep_for(10ms);
        }

        return std::vector<milliseconds>();
    }

    void printResults(const std::vector<milliseconds>& v)
    {
        for (const auto p : v)
            qDebug() << p.count();
    }

private:
    std::vector<milliseconds> m_handlerCalls;
    nx::Mutex m_mutex;
    nx::utils::ElapsedTimer m_elapsedTimer;
};

// We test here that runner executes tasks and stops without errors and forced tasks are executed
// out of timer order.
// It's error prone and difficult to assert anything in thread related
// tests without access to the interiors of the class being tested. So there is only debug
// printing code instead.

TEST_F(StorageCheckRunnerTest, PeriodicTasks)
{
    const auto callResults = waitForCallsAndGetResults(kExpectedCallCount);
    #if 0 //< Test results report. For debugging purposes.
        printResults(callResults);
    #endif
}

TEST_F(StorageCheckRunnerTest, ForceExec)
{
    std::this_thread::sleep_for(100ms);
    runner.forceCheck();

    std::this_thread::sleep_for(100ms);
    runner.forceCheck();

    const auto callResults = waitForCallsAndGetResults(kExpectedCallCount + 1);
    #if 0 //< Test results report. For debugging purposes.
        printResults(callResults);
    #endif
}

} // namespace nx::vms::server::test