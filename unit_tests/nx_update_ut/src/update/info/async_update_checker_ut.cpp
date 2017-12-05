#include <functional>
#include <queue>
#include <gtest/gtest.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/random.h>
#include <nx/update/info/async_update_checker.h>
#include <nx/update/info/detail/data_provider/raw_data_provider_factory.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider_handler.h>

#include "detail/json_data.h"

namespace nx {
namespace update {
namespace info {
namespace test {

static const QString kBaseUrl = "http://updates.networkoptix.com";

class SingleThreadExecutor
{
public:
    SingleThreadExecutor()
    {
        m_needToStop = false;
        m_thread = utils::thread(std::bind(&SingleThreadExecutor::runLoop, this));
    }

    ~SingleThreadExecutor()
    {
        stop();
        m_thread.join();
    }

    void submit(utils::MoveOnlyFunc<void()> task)
    {
        QnMutexLocker lock(&m_mutex);
        m_tasks.emplace(std::move(task));
        m_condition.wakeOne();
    }

private:
    utils::thread m_thread;
    std::queue<utils::MoveOnlyFunc<void()>> m_tasks;
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;
    bool m_needToStop;

    void runLoop()
    {
        while (!needStop())
            runLoopIteration();
    }

    bool needStop() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_needToStop;
    }

    void stop()
    {
        QnMutexLocker lock(&m_mutex);
        m_needToStop = true;
        m_condition.wakeOne();
    }

    void runLoopIteration()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_needToStop && m_tasks.empty())
            m_condition.wait(lock.mutex());

        if (m_needToStop)
            return;

        while (!m_tasks.empty() && !m_needToStop)
            runTask(&lock);
    }

    void runTask(QnMutexLockerBase* lock)
    {
        utils::MoveOnlyFunc<void()> task = std::move(m_tasks.front());
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
        const QString& customization,
        const QString& version) override
    {
        using namespace detail::data_provider;
        using namespace std::placeholders;

        ResponseComposer responseComposer(customization, version);
        m_executor.submit(
            std::bind(
                &AbstractAsyncRawDataProviderHandler::onGetSpecificUpdateInformationDone,
                m_handler,
                responseComposer.resultCode(),
                responseComposer.responseData()));
    }

private:
    class ResponseComposer
    {
    public:
        ResponseComposer(const QString& customization, const QString& version)
        {
            auto testUpdateIt = std::find_if(
                updateTestDataList.cbegin(),
                updateTestDataList.cend(),
                [&customization, &version](const UpdateTestData& updateTestData)
                {
                    return updateTestData.customization == customization
                        && updateTestData.version == version;
                });

            if (testUpdateIt == updateTestDataList.cend())
            {
                chooseRandomResultCode();
                return;
            }

            m_responseData = testUpdateIt->json;
        }

        ResultCode resultCode() const { return m_resultCode; }
        QByteArray responseData() const { return m_responseData; }
    private:
        ResultCode m_resultCode = ResultCode::ok;
        QByteArray m_responseData;

        void chooseRandomResultCode()
        {
            m_resultCode = nx::utils::random::number(0, 1) == 0
                ? ResultCode::getRawDataError
                : ResultCode::ok;
        }
    };

    detail::data_provider::AbstractAsyncRawDataProviderHandler* m_handler = nullptr;
    SingleThreadExecutor m_executor;
};

class AsyncUpdateChecker: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
    }

    virtual void TearDown() override
    {
        using namespace detail::data_provider;
        RawDataProviderFactory::setFactoryFunction(nullptr);
    }

    void whenMockupDataProviderHasBeenSetUp() const
    {
        using namespace detail::data_provider;
        RawDataProviderFactory::setFactoryFunction(
            [](const QString& s, AbstractAsyncRawDataProviderHandler* handler)
            {
                return std::make_unique<AsyncDataProviderMockUp>(s, handler);
            });
    }

    void whenAsyncCheckRequestHasBeenIssued()
    {
        using namespace std::placeholders;
        m_asyncUpdateChecker.check(
            kBaseUrl,
            std::bind(&AsyncUpdateChecker::onCheckCompleted, this, _1, _2));
    }

    void thenCorrectUpdateRegistryShouldBeReturned()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_done)
            m_condition.wait(lock.mutex());

        ASSERT_EQ(ResultCode::ok, m_resultCode);
        ASSERT_TRUE(m_updateRegistry);
        assertUpdateRegistryContent();
    }

private:
    info::AsyncUpdateChecker m_asyncUpdateChecker;
    AbstractUpdateRegistryPtr m_updateRegistry;
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    bool m_done = false;
    ResultCode m_resultCode = ResultCode::noData;

    void onCheckCompleted(ResultCode resultCode, AbstractUpdateRegistryPtr updateRegistry)
    {
        QnMutexLocker lock(&m_mutex);
        m_resultCode = resultCode;
        m_done = true;
        m_updateRegistry = std::move(updateRegistry);
        m_condition.wakeOne();
    }

    void assertUpdateRegistryContent() const
    {
        assertAlternativeServer();
        assertFileDataContent();
    }

    void assertAlternativeServer() const
    {
        ASSERT_EQ(1, m_updateRegistry->alternativeServers().size());
        ASSERT_FALSE(m_updateRegistry->alternativeServers()[0].isEmpty());
    }

    void assertFileDataContent() const
    {
        //m_updateRegistry->findUpdate()
    }
};

TEST_F(AsyncUpdateChecker, AsyncOperationCompletesSuccessfully)
{
    whenMockupDataProviderHasBeenSetUp();
    whenAsyncCheckRequestHasBeenIssued();
    thenCorrectUpdateRegistryShouldBeReturned();
}

} // namespace test
} // namespace info
} // namespace update
} // namespace nx
