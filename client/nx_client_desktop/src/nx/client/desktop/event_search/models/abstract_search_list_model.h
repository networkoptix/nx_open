#pragma once

#include "abstract_event_list_model.h"

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

namespace nx::client::desktop {

/**
 * Abstract Right Panel data model that provides interface and basic mechanics of sliding window
 * synchronous or asynchronous lookup into underlying data store.
 */
class AbstractSearchListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

public:
    explicit AbstractSearchListModel(QObject* parent = nullptr);
    virtual ~AbstractSearchListModel() override = default;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

    QnVirtualCameraResourceSet cameras() const;
    void setCameras(const QnVirtualCameraResourceSet& value);

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

    /** Time window fetched at this moment. */
    QnTimePeriod fetchedTimeWindow() const;

    /** Fully reset model. */
    void clear();

    /** Returns whether the model with current settings performs any data filtering. */
    virtual bool isConstrained() const;

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
    void fetchFinished(FetchResult result, QPrivateSignal);
    void liveChanged(bool isLive, QPrivateSignal);
    void livePausedChanged(bool isPaused, QPrivateSignal);
    void camerasAboutToBeChanged(QPrivateSignal);
    void camerasChanged(QPrivateSignal);

protected:
    // These functions must be overridden in derived classes.

    /** Returns whether fetch can be started and there's more data to fetch in selected direction. */
    virtual bool canFetch() const = 0;

    /** Requests next data fetch in selected direction. */
    virtual void requestFetch() = 0;

    /** Clear all fetched data. Doesn't need to reset fetched time window. */
    virtual void clearData() = 0;

    /** Truncate fetched data to currently selected relevant time period.
     * Doesn't need to reset fetched time window. */
    virtual void truncateToRelevantTimePeriod() = 0;

    /** Truncate fetched data to maximal item count at front or back,
     * depending on current fetch direction. Must update fetched time window correspondingly. */
    virtual void truncateToMaximumCount() = 0;

    /** Should be called during every successful fetch to update fetched time window. */
    void setFetchedTimeWindow(const QnTimePeriod& value);

    /** Should be called at the end of every fetch to emit fetchFinished signal. */
    void finishFetch(FetchResult result);

    /** Sets whether underlying data store can be populated with new items in live mode. */
    void setLiveSupported(bool value);

private:
    QnTimePeriod m_relevantTimePeriod = QnTimePeriod::anytime(); //< Time period of interest.
    int m_maximumCount = 1000; //< Maximum item count.
    int m_fetchBatchSize = 100; //< Item count acquired by one fetch.
    bool m_liveSupported = false; //< Whether underlying data store can be updated in live.
    bool m_live = false; //< Live mode enabled state.
    bool m_livePaused = false; //< Live mode paused state.

    FetchDirection m_fetchDirection = FetchDirection::earlier; //< Direction for next fetch.

    QnTimePeriod m_fetchedTimeWindow; //< Time window of currently fetched data.

    QnVirtualCameraResourceSet m_cameras; //< Relevant cameras.
};

} // namespace nx::client::desktop

Q_DECLARE_METATYPE(nx::client::desktop::AbstractSearchListModel::FetchResult)
