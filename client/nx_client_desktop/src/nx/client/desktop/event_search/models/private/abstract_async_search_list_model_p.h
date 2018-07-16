#pragma once

#include "../abstract_async_search_list_model.h"

#include <limits>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class AbstractAsyncSearchListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(AbstractAsyncSearchListModel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    virtual void setCamera(const QnVirtualCameraResourcePtr& camera);

    void relevantTimePeriodChanged(const QnTimePeriod& previousValue);
    void fetchDirectionChanged();

    virtual int count() const = 0;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const = 0;

    virtual void clear();
    virtual bool requestFetch();

    using PrefetchCompletionHandler = std::function<void(const QnTimePeriod& fetchedPeriod)>;

    bool canFetchMore() const;
    bool prefetch(PrefetchCompletionHandler completionHandler);
    void commit(const QnTimePeriod& periodToCommit);
    bool fetchInProgress() const;

    void cancelPrefetch();

    QnTimePeriod fetchedTimeWindow() const;

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) = 0;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit, bool& fetchedAll) = 0;
    virtual void clipToSelectedTimePeriod() = 0;
    virtual bool hasAccessRights() const = 0;

    bool shouldSkipResponse(rest::Handle requestId) const;
    void completePrefetch(const QnTimePeriod& actuallyFetched, bool limitReached);

    template<class DataContainer, class UpperBoundPredicate>
    void clipToTimePeriod(DataContainer& data,
        UpperBoundPredicate upperBoundPredicate,
        const QnTimePeriod& period,
        std::function<void(typename DataContainer::const_reference)> itemCleanup = nullptr);

    int lastBatchSize() const;

private:
    AbstractAsyncSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    QnTimePeriod m_fetchedTimeWindow;
    rest::Handle m_currentFetchId = rest::Handle();
    PrefetchCompletionHandler m_prefetchCompletionHandler;
    FetchDirection m_prefetchDirection = FetchDirection::earlier;
    QnTimePeriod m_requestedFetchPeriod;
    bool m_fetchedAll = false;
    int m_lastBatchSize = 0;
};

// ------------------------------------------------------------------------------------------------
// Template method implementation.

template<class DataContainer, class UpperBoundPredicate>
void AbstractAsyncSearchListModel::Private::clipToTimePeriod(
    DataContainer& data,
    UpperBoundPredicate upperBoundPredicate,
    const QnTimePeriod& period,
    std::function<void(typename DataContainer::const_reference)> itemCleanup)
{
    if (data.empty())
        return;

    // Remove records later than end of the period.
    const auto frontEnd = std::upper_bound(data.begin(), data.end(),
        period.endTimeMs(), upperBoundPredicate);

    const auto frontLength = std::distance(data.begin(), frontEnd);
    if (frontLength != 0)
    {
        ScopedRemoveRows removeRows(q, 0, frontLength - 1);
        if (itemCleanup)
            std::for_each(data.begin(), frontEnd, itemCleanup);
        data.erase(data.begin(), frontEnd);
    }

    // Remove records earlier than start of the period.
    const auto tailBegin = std::upper_bound(data.begin(), data.end(),
        period.startTimeMs, upperBoundPredicate);

    const auto tailLength = std::distance(tailBegin, data.end());
    if (tailLength != 0)
    {
        const auto tailStart = std::distance(data.begin(), tailBegin);
        ScopedRemoveRows removeRows(q, tailStart, tailStart + tailLength - 1);
        if (itemCleanup)
            std::for_each(tailBegin, data.end(), itemCleanup);
        data.erase(tailBegin, data.end());
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
