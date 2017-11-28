#pragma once

#include <atomic>
#include <memory>

#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>

#include "../request_executor.h"

namespace nx {
namespace utils {
namespace db {
namespace detail {

class NX_UTILS_API QueryQueue:
    private nx::utils::SyncQueueWithItemStayTimeout<std::unique_ptr<AbstractExecutor>>
{
    using base_type =
        nx::utils::SyncQueueWithItemStayTimeout<std::unique_ptr<AbstractExecutor>>;

public:
    QueryQueue();

    using base_type::enableItemStayTimeoutEvent;
    using base_type::push;
    using base_type::generateReaderId;
    using base_type::size;
    using base_type::addReaderToTerminatedList;
    using base_type::removeReaderFromTerminatedList;

    /**
     * @param value Zero - no limit. By default, zero.
     */
    void setConcurrentModificationQueryLimit(int value);

    boost::optional<std::unique_ptr<AbstractExecutor>> pop(
        boost::optional<std::chrono::milliseconds> timeout = boost::none,
        QueueReaderId readerId = kInvalidQueueReaderId);

private:
    std::atomic<int> m_currentModificationCount;
    int m_concurrentModificationQueryLimit = 0;

    void decreaseLimitCounters(AbstractExecutor* finishedQuery);
    bool checkAndUpdateQueryLimits(const std::unique_ptr<AbstractExecutor>& queryExecutor);
};

} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
