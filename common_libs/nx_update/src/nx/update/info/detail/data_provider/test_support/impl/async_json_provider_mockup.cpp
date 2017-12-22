#include <queue>
#include <functional>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/random.h>
#include <nx/utils/std/thread.h>

#include "async_json_provider_mockup.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {
namespace test_support {

using namespace data_provider::test_support;

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

    SingleThreadExecutor(const SingleThreadExecutor&) = delete;
    SingleThreadExecutor& operator = (const SingleThreadExecutor&) = delete;
    SingleThreadExecutor(SingleThreadExecutor&&) = delete;
    SingleThreadExecutor& operator = (SingleThreadExecutor&&) = delete;

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
        const auto task = std::move(m_tasks.front());
        m_tasks.pop();
        lock->unlock();
        task();
        lock->relock();
    }
};

class ResponseComposer
{
public:
    ResponseComposer(
        const QString& customization,
        const QString& version,
        const std::vector<UpdateTestData>& updateTestDataList)
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

AsyncJsonProviderMockup::AsyncJsonProviderMockup(AbstractAsyncRawDataProviderHandler* handler):
    m_handler(handler),
    m_executor(new SingleThreadExecutor)
{}

void AsyncJsonProviderMockup::getUpdatesMetaInformation()
{
    using namespace detail::data_provider;
    m_executor->submit(
        std::bind(
            &AbstractAsyncRawDataProviderHandler::onGetUpdatesMetaInformationDone,
            m_handler, ResultCode::ok, metaDataJson()));
}

void AsyncJsonProviderMockup::getSpecificUpdateData(
    const QString& customization,
    const QString& version)
{
    using namespace detail::data_provider;
    using namespace std::placeholders;

    ResponseComposer responseComposer(customization, version, updateTestDataList());
    m_executor->submit(
        std::bind(
            &AbstractAsyncRawDataProviderHandler::onGetSpecificUpdateInformationDone,
            m_handler,
            responseComposer.resultCode(),
            responseComposer.responseData()));
}

AsyncJsonProviderMockup::~AsyncJsonProviderMockup() = default;

} // namespace test_support
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
