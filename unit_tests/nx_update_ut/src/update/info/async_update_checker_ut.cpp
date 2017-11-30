#include <functional>
#include <queue>
#include <gtest/gtest.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/update/info/async_update_checker.h>
#include <nx/update/info/abstract_update_registry.h>
#include <nx/update/info/detail/data_provider/raw_data_provider_factory.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider_handler.h>

#include "../../inl.h"

namespace nx {
namespace update {
namespace info {
namespace test {

static const QString kDummyBaseUrl = "dummy";

class SingleThreadExecutor
{
public:
    SingleThreadExecutor()
    {
        m_needToStop = false;
        m_thread = nx::utils::thread(std::bind(&SingleThreadExecutor::runLoop, this));
    }

    ~SingleThreadExecutor()
    {
        m_needToStop = true;
        m_thread.join();
    }

    void submit(nx::utils::MoveOnlyFunc<void()> task)
    {
        QnMutexLocker lock(&m_mutex);
        m_tasks.emplace(std::move(task));
        m_condition.wakeOne();
    }

private:
    nx::utils::thread m_thread;
    std::queue<nx::utils::MoveOnlyFunc<void()>> m_tasks;
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::atomic<bool> m_needToStop;

    void runLoop()
    {
        while (!m_needToStop)
            runLoopIteration();
    }

    void runLoopIteration()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_needToStop && m_tasks.empty())
            m_condition.wait(lock.mutex());

        if (m_needToStop)
            return;

        while (!m_tasks.empty() && !m_needToStop)
            runTasks(&lock);
    }

    void runTasks(QnMutexLockerBase* lock)
    {
        nx::utils::MoveOnlyFunc<void()> task = std::move(m_tasks.front());
        m_tasks.pop();
        lock->unlock();
        task();
        lock->relock();
    }
};

class AsyncDataProviderMockUp: public detail::data_provider::AbstractAsyncRawDataProvider
{
public:
    AsyncDataProviderMockUp(
        const QString& /*url*/,
        detail::data_provider::AbstractAsyncRawDataProviderHandler* handler)
        :
        m_handler(handler)
    {}

    virtual void getUpdatesMetaInformation() override
    {
        using namespace detail::data_provider;
        m_executor.submit(
            std::bind(
                &AbstractAsyncRawDataProviderHandler::onGetUpdatesMetaInformationDone,
                m_handler, ResultCode::ok, metaDataJson));
    }

    virtual void getSpecificUpdateData(
        const QString& /*customization*/,
        const QString& /*version*/) override
    {
        using namespace detail::data_provider;
        using namespace std::placeholders;

        m_executor.submit(
            std::bind(
                &AbstractAsyncRawDataProviderHandler::onGetSpecificUpdateInformationDone,
                m_handler, ResultCode::ok, updateJson));
    }

private:
    detail::data_provider::AbstractAsyncRawDataProviderHandler* m_handler = nullptr;
    SingleThreadExecutor m_executor;
};

class AsyncUpdateChecker: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        using namespace detail::data_provider;
        RawDataProviderFactory::setFactoryFunction(
            [](const QString& s, AbstractAsyncRawDataProviderHandler* handler)
            {
                return std::make_unique<AsyncDataProviderMockUp>(s, handler);
            });
    }

    virtual void TearDown() override
    {
        using namespace detail::data_provider;
        RawDataProviderFactory::setFactoryFunction(nullptr);
    }

    void whenAsyncCheckRequestHasBeenIssued()
    {
        using namespace std::placeholders;
        m_asyncUpdateChecker.check(
            kDummyBaseUrl,
            std::bind(&AsyncUpdateChecker::onCheckCompleted, this, _1, _2));
    }

    void thenNonEmptyAbstractUpdateRegistryPtrShouldBeReturned()
    {
        QnMutexLocker lock(&m_mutex);
        while (!(bool) m_updateRegistry)
            m_condition.wait(lock.mutex());

        ASSERT_TRUE((bool) m_updateRegistry);
    }
private:
    info::AsyncUpdateChecker m_asyncUpdateChecker;
    AbstractUpdateRegistryPtr m_updateRegistry;
    QnMutex m_mutex;
    QnWaitCondition m_condition;

    void onCheckCompleted(ResultCode resultCode, AbstractUpdateRegistryPtr updateRegistry)
    {
        ASSERT_EQ(ResultCode::ok, resultCode);
        QnMutexLocker lock(&m_mutex);
        m_updateRegistry = std::move(updateRegistry);
        m_condition.wakeOne();
    }
};

TEST_F(AsyncUpdateChecker, AsyncOperationCompletesSuccessfully)
{
    whenAsyncCheckRequestHasBeenIssued();
    thenNonEmptyAbstractUpdateRegistryPtrShouldBeReturned();
}

} // namespace test
} // namespace info
} // namespace update
} // namespace nx
