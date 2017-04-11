#include <gtest/gtest.h>

#include <thread>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/sync_queue.h>

namespace nx {
namespace utils {
namespace test {

class AsyncRunner
{
public:
    AsyncRunner()
    {
        m_thread = nx::utils::thread([this]()
        {
            while(1)
            {
                if (const auto handler = m_tasks.pop())
                    handler();
                else
                    return;

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void addTask(std::function<void()> task)
    {
        m_tasks.push(std::move(task));
    }

    ~AsyncRunner()
    {
        m_tasks.push(nullptr);
        m_thread.join();
    }

private:
    utils::thread m_thread;
    utils::TestSyncQueue<std::function<void()>> m_tasks;
};

class AsyncOperationGuardTest
{
public:
    AsyncOperationGuardTest(AsyncRunner* runner)
        : m_runner(runner)
    {
    }

    void startOperation()
    {
        auto& sharedGuard = m_operationGuard.sharedGuard();
        m_runner->addTask([sharedGuard, this](){
            if (const auto lock = sharedGuard->lock())
                m_completeQueue.push(true);
        });
    }

    void waitOperationComplete()
    {
        m_completeQueue.pop();
    }

private:
    AsyncRunner* m_runner;
    utils::TestSyncQueue<bool> m_completeQueue;
    AsyncOperationGuard m_operationGuard;
};

static const size_t kOperationsCount = 5;
static const size_t kCompleteOperationsCount = 2;
static const size_t kTestRepeats = 2;

TEST(AsyncOperationGuard, Test)
{
    AsyncRunner runner;

    for (size_t tc = 0; tc < kTestRepeats; ++tc)
    {
        AsyncOperationGuardTest asyncTest(&runner);

        for (size_t oc = 0; oc < kOperationsCount; ++oc)
            asyncTest.startOperation();

        for (size_t oc = 0; oc < kCompleteOperationsCount; ++oc)
            asyncTest.waitOperationComplete();
    }
}

} // namespace test
} // namespace utils
} // namespace nx
