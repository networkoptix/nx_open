#pragma once

#include "abstract_event_list_model.h"

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {

class ManagedCameraSet;

/**
 * Abstract Right Panel data model that provides interface and basic mechanics of sliding window
 * synchronous or asynchronous lookup into underlying data store.
 */
class AbstractSearchListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

public:
    explicit AbstractSearchListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~AbstractSearchListModel() override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

    ManagedCameraSet* cameraSet() const;
    QnVirtualCameraResourceSet cameras() const;
    bool isOnline() const; //< Connected to server, initial resources received.

    enum class FetchDirection
    {
        earlier,
        later
    };
    Q_ENUM(FetchDirection)

    /** In which direction fetchMore expands or shifts fetched data window. */
    FetchDirection fetchDirection() const;
    void setFetchDirection(FetchDirection value);

    /** Time period of interest. The model will not fetch any data outside of this period. */
    const QnTimePeriod& relevantTimePeriod() const;
    void setRelevantTimePeriod(const QnTimePeriod& value);

    /** Maximal fetched window size. */
    int maximumCount() const;
    void setMaximumCount(int value);

    /** Item count acquired by one fetch. */
    int fetchBatchSize() const;
    void setFetchBatchSize(int value);

    /** Whether underlying data store can be populated with new items in live mode. */
    bool liveSupported() const;
    bool effectiveLiveSupported() const; //< liveSupported() && relevantTimePeriod().isInfinite()

    /** Whether the model is in live mode, i.e. receives newly happening events in realtime. */
    bool isLive() const;
    void setLive(bool value);

    /** Whether live updates are paused, e.g. when the model's item view is invisible. */
    bool livePaused() const;
    void setLivePaused(bool value);

    /** Whether the model can work without server connection, for example with local files. */
    bool offlineAllowed() const;
    void setOfflineAllowed(bool value);

    /** Time window fetched at this moment. */
    QnTimePeriod fetchedTimeWindow() const;

    /** Fully reset model. */
    void clear();

    /** Returns whether the model with current settings performs any data filtering. */
    virtual bool isConstrained() const;

    /** Returns whether current user has enough access rights to use this model. */
    virtual bool hasAccessRights() const;

    // The following functions should be overridden in asynchronous fetch models.
    virtual bool fetchInProgress() const;
    virtual bool cancelFetch();

    enum class FetchResult
    {
        complete, //< Successful. There's no more data to fetch.
        incomplete, //< Successful. There's more data to fetch.
        failed, //< Unsuccessful.
        cancelled //< Cancelled.
    };
    Q_ENUM(FetchResult)

signals:
    void fetchCommitStarted(FetchDirection direction, QPrivateSignal);
    void fetchFinished(FetchResult result, QPrivateSignal);
    void liveChanged(bool isLive, QPrivateSignal);
    void livePausedChanged(bool isPaused, QPrivateSignal);
    void camerasAboutToBeChanged(QPrivateSignal);
    void camerasChanged(QPrivateSignal);
    void isOnlineChanged(bool isOnline, QPrivateSignal);
    void dataNeeded();

protected:
    // Fetch (non-live) commit must be guarded by this object.
    // It emits fetchCommitStarted/fetchFinished signals.
    struct ScopedFetchCommit: public nx::utils::SharedGuard
    {
        explicit ScopedFetchCommit(
            AbstractSearchListModel* model, FetchDirection direction, FetchResult result)
            :
            nx::utils::SharedGuard(nx::utils::SharedGuardCallback(
                [model, result]() { emit model->fetchFinished(result, {}); }))
        {
            emit model->fetchCommitStarted(direction, {});
        }
    };

protected:
    // These functions must be overridden in derived classes.

    /** Returns whether fetch can be started at this moment.
     * It shouldn't check whether if can be started at all.
     * For example, asynchronous fetch models should check here if fetch is in progress. */
    virtual bool canFetchNow() const;

    /** Requests next data fetch in selected direction. */
    virtual void requestFetch() = 0;

    /** Clear all fetched data. Doesn't need to reset fetched time window. */
    virtual void clearData() = 0;

    /** Truncate fetched data to currently selected relevant time period.
     * Doesn't need to reset fetched time window. */
    virtual void truncateToRelevantTimePeriod() = 0;

    template<class Item>
    using GetTimestampFunction =
        std::function<std::chrono::milliseconds(const Item&)>;

    template<class Item>
    using ItemCleanupFunction = std::function<void(const Item&)>;

    // Helper to truncate data sorted in descending timestamp order.
    template<class DataContainer>
    void truncateDataToTimePeriod(DataContainer& data,
        GetTimestampFunction<typename DataContainer::value_type> getTimestamp,
        const QnTimePeriod& period,
        ItemCleanupFunction<typename DataContainer::value_type> itemCleanup = nullptr);

    /** Truncate fetched data to maximal item count at front or back,
     * depending on current fetch direction. Must update fetched time window correspondingly. */
    virtual void truncateToMaximumCount() = 0;

    // Helper to truncate data sorted in descending timestamp order.
    template<class DataContainer>
    void truncateDataToMaximumCount(DataContainer& data,
        GetTimestampFunction<typename DataContainer::value_type> getTimestamp,
        ItemCleanupFunction<typename DataContainer::value_type> itemCleanup = nullptr);

    /** Returns whether specified camera is applicable for this model. */
    virtual bool isCameraApplicable(const QnVirtualCameraResourcePtr& camera) const;

    /** Should be called during every successful fetch to update fetched time window. */
    void setFetchedTimeWindow(const QnTimePeriod& value);
    void addToFetchedTimeWindow(const QnTimePeriod& period); //< Just a helper function.

    /** Sets whether underlying data store can be populated with new items in live mode. */
    void setLiveSupported(bool value);

    /** Checks whether filter is degenerate and fetch will never return any results. */
    virtual bool isFilterDegenerate() const;

private:
    QnTimePeriod m_relevantTimePeriod = QnTimePeriod::anytime(); //< Time period of interest.
    int m_maximumCount = 500; //< Maximum item count.
    int m_fetchBatchSize = 150; //< Item count acquired by one fetch.
    bool m_liveSupported = false; //< Whether underlying data store can be updated in live.
    bool m_live = false; //< Live mode enabled state.
    bool m_livePaused = false; //< Live mode paused state.
    bool m_isOnline = false; //< Whether connection with server is fully established.
    bool m_offlineAllowed = false; //< Whether the model can work without server connection.

    FetchDirection m_fetchDirection = FetchDirection::earlier; //< Direction for next fetch.

    QnTimePeriod m_fetchedTimeWindow; //< Time window of currently fetched data.

    const QScopedPointer<ManagedCameraSet> m_cameraSet; //< Relevant camera set.
};

//-------------------------------------------------------------------------------------------------
// Template method implementation.

template<class DataContainer>
void AbstractSearchListModel::truncateDataToTimePeriod(
    DataContainer& data, //< Must be sorted in descending order by timestamp!
    GetTimestampFunction<typename DataContainer::value_type> getTimestamp,
    const QnTimePeriod& period,
    ItemCleanupFunction<typename DataContainer::value_type> itemCleanup)
{
    using namespace std::chrono;

    if (data.empty())
        return;

    const auto upperBoundPredicate =
        [getTimestamp](std::chrono::milliseconds left, const auto& right)
        {
            return left > getTimestamp(right);
        };

    // Remove records later than end of the period.
    if (!period.isInfinite())
    {
        const auto frontEnd = std::upper_bound(data.begin(), data.end(),
            period.endTime() + 1ms, upperBoundPredicate);

        const auto frontLength = std::distance(data.begin(), frontEnd);
        if (frontLength != 0)
        {
            ScopedRemoveRows removeRows(this, 0, frontLength - 1);
            if (itemCleanup)
                std::for_each(data.begin(), frontEnd, itemCleanup);
            data.erase(data.begin(), frontEnd);
        }
    }

    // Remove records earlier than start of the period.
    const auto tailBegin = std::upper_bound(data.begin(), data.end(),
        period.startTime(), upperBoundPredicate);

    const auto tailLength = std::distance(tailBegin, data.end());
    if (tailLength != 0)
    {
        const auto tailStart = std::distance(data.begin(), tailBegin);
        ScopedRemoveRows removeRows(this, tailStart, tailStart + tailLength - 1);
        if (itemCleanup)
            std::for_each(tailBegin, data.end(), itemCleanup);
        data.erase(tailBegin, data.end());
    }
}

template<class DataContainer>
void AbstractSearchListModel::truncateDataToMaximumCount(
    DataContainer& data, //< Must be sorted in descending order by timestamp!
    GetTimestampFunction<typename DataContainer::value_type> getTimestamp,
    ItemCleanupFunction<typename DataContainer::value_type> itemCleanup)
{
    if (maximumCount() <= 0)
        return;

    const int toRemove = int(data.size()) - maximumCount();
    if (toRemove <= 0)
        return;

    if (fetchDirection() == FetchDirection::earlier)
    {
        ScopedRemoveRows removeRows(this, 0, toRemove - 1);
        const auto removeEnd = data.begin() + toRemove;
        if (itemCleanup)
            std::for_each(data.begin(), removeEnd, itemCleanup);

        data.erase(data.begin(), removeEnd);

        // If top is truncated, go out of live mode.
        setLive(false);
    }
    else
    {
        NX_ASSERT(fetchDirection() == FetchDirection::later);
        const auto index = int(data.size()) - toRemove;
        const auto removeBegin = data.begin() + index;

        ScopedRemoveRows removeRows(this, index, index + toRemove - 1);
        if (itemCleanup)
            std::for_each(removeBegin, data.end(), itemCleanup);

        data.erase(removeBegin, data.end());
    }

    auto timeWindow = fetchedTimeWindow();
    if (fetchDirection() == FetchDirection::earlier)
        timeWindow.truncate(getTimestamp(data.front()).count());
    else
        timeWindow.truncateFront(getTimestamp(data.back()).count());

    setFetchedTimeWindow(timeWindow);
}

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::AbstractSearchListModel::FetchResult)
