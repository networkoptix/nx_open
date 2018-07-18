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

    virtual int count() const = 0;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const = 0;

    virtual void clearData() = 0;
    virtual void truncateToMaximumCount() = 0;
    virtual void truncateToRelevantTimePeriod() = 0;

    bool canFetch() const;
    bool requestFetch();
    bool fetchInProgress() const;
    void cancelPrefetch();

    using PrefetchCompletionHandler = std::function<void(const QnTimePeriod& fetchedPeriod)>;
    bool prefetch(PrefetchCompletionHandler completionHandler);
    void commit(const QnTimePeriod& periodToCommit);

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) = 0;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) = 0;
    virtual bool hasAccessRights() const = 0;

    void completePrefetch(const QnTimePeriod& actuallyFetched, int fetchedBatchSize);

    template<class DataContainer, class UpperBoundPredicate>
    void truncateDataToTimePeriod(DataContainer& data,
        UpperBoundPredicate upperBoundPredicate,
        const QnTimePeriod& period,
        std::function<void(typename DataContainer::const_reference)> itemCleanup = nullptr);

    template<class DataContainer>
    void truncateDataToCount(DataContainer& data, int count,
        std::function<void(typename DataContainer::const_reference)> itemCleanup = nullptr);

    struct FetchInformation
    {
        rest::Handle id = rest::Handle();
        FetchDirection direction = FetchDirection::earlier;
        QnTimePeriod period;
        int batchSize = 0;
    };

    const FetchInformation& currentRequest() const;

private:
    AbstractAsyncSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;

    PrefetchCompletionHandler m_prefetchCompletionHandler;
    FetchInformation m_request;
};

// ------------------------------------------------------------------------------------------------
// Template method implementation.

template<class DataContainer, class UpperBoundPredicate>
void AbstractAsyncSearchListModel::Private::truncateDataToTimePeriod(
    DataContainer& data,
    UpperBoundPredicate upperBoundPredicate,
    const QnTimePeriod& period,
    std::function<void(typename DataContainer::const_reference)> itemCleanup)
{
    if (data.empty())
        return;

    // Remove records later than end of the period.
    const auto frontEnd = std::upper_bound(data.begin(), data.end(),
        period.endTime(), upperBoundPredicate);

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
        period.startTime(), upperBoundPredicate);

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

template<class DataContainer>
void AbstractAsyncSearchListModel::Private::truncateDataToCount(DataContainer& data, int count,
    std::function<void(typename DataContainer::const_reference)> itemCleanup)
{
    const int toRemove = int(data.size()) - count;
    if (toRemove <= 0)
        return;

    NX_ASSERT(count > 0);
    if (count <= 0)
    {
        q->clear();
        return;
    }

    if (q->fetchDirection() == FetchDirection::earlier)
    {
        ScopedRemoveRows removeRows(q, 0, toRemove - 1);
        const auto removeEnd = data.begin() + toRemove;
        if (itemCleanup)
            std::for_each(data.begin(), removeEnd, itemCleanup);

        data.erase(data.begin(), removeEnd);
    }
    else
    {
        NX_ASSERT(q->fetchDirection() == FetchDirection::later);
        const auto index = int(data.size()) - toRemove;
        const auto removeBegin = data.begin() + index;

        ScopedRemoveRows removeRows(q, index, index + toRemove - 1);
        if (itemCleanup)
            std::for_each(removeBegin, data.end(), itemCleanup);

        data.erase(removeBegin, data.end());
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
