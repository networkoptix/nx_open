#pragma once

#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace utils {

// Every test MUST be finite!
static const std::chrono::minutes kTestSyncQueueTimeout(1);

template<typename Base>
class TestSyncQueueBase:
    public Base
{
public:
    explicit TestSyncQueueBase(
        std::chrono::milliseconds timeout = kTestSyncQueueTimeout)
    :
        m_timeout(timeout)
    {
    }

    void timedOut()
    {
        GTEST_FAIL() << "TestSyncQueue::pop() has timed out";
    }

    typename Base::ResultType pop() /* overlap */
    {
        auto value = Base::pop(m_timeout);
        if (!value)
        {
            timedOut();
            return typename Base::ResultType();
        }

        return std::move(*value);
    }

private:
    std::chrono::milliseconds m_timeout;
};

template<typename Result>
using TestSyncQueue = TestSyncQueueBase<SyncQueue<Result>>;

template<typename ... Results>
using TestSyncMultiQueue = TestSyncQueueBase<SyncMultiQueue<Results ...>>;

} // namespace utils
} // namespace nx
