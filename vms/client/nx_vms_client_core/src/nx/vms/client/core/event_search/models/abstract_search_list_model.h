// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/event_search/models/abstract_event_list_model.h>
#include <recording/time_period.h>
#include <nx/utils/impl_ptr.h>
#include <recording/time_period.h>

#include "fetch_request.h"

namespace nx::vms::client::core {

struct FetchedDataRanges;
class UserWatcher;
class ManagedCameraSet;
class TextFilterSetup;
class SystemContext;

/**
 * Abstract Event Search data model that provides interface and basic mechanics of sliding window
 * synchronous or asynchronous lookup into underlying data store.
 */
class NX_VMS_CLIENT_CORE_API AbstractSearchListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

public:
    static void registerQmlType();

    static constexpr int kStandardMaximumItemCount = 300;

    explicit AbstractSearchListModel(
        int maximumCount,
        SystemContext* systemContext,
        QObject* parent = nullptr);

    explicit AbstractSearchListModel(
        SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual ~AbstractSearchListModel() override;

    virtual bool fetchData(const FetchRequest& request);

    Q_INVOKABLE bool fetchInProgress() const;
    bool canFetchData(EventSearch::FetchDirection fetchDirection) const;
    OptionalFetchRequest lastFetchRequest() const;
    Q_INVOKABLE void clear();

    const ManagedCameraSet& cameraSet() const;
    ManagedCameraSet& cameraSet();
    bool isOnline() const; //< Connected to server, initial resources received.

    /** Time period of interest. The model will not fetch any data outside of this period. */
    OptionalTimePeriod interestTimePeriod() const;
    void setInterestTimePeriod(const OptionalTimePeriod& value);

    /** Whether underlying data store can be populated with new items in live mode. */
    bool liveSupported() const;
    bool effectiveLiveSupported() const;

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
    OptionalTimePeriod fetchedTimeWindow() const;

    /** Returns whether the model with current settings performs any data filtering. */
    virtual bool isConstrained() const;

    /** Returns whether current user has enough access rights to use this model. */
    virtual bool hasAccessRights() const;

    /** Returns text filter for this model or null if text filtering is not supported (default). */
    virtual TextFilterSetup* textFilter() const;

    /** Optimized find within a fetched window (logarithmic complexity). */
    std::pair<int, int> find(std::chrono::milliseconds timestamp) const;

    int maximumCount() const;

    Q_INVOKABLE FetchRequest requestForDirection(
        EventSearch::FetchDirection direction) const;

signals:
    void fetchCommitStarted(const FetchRequest& request);
    void fetchFinished(
        EventSearch::FetchResult result,
        int centralItemIndex,
        const FetchRequest& request); //< TODO : make fetch finished private???
    void liveChanged(bool isLive, QPrivateSignal);
    void livePausedChanged(bool isPaused, QPrivateSignal);
    void liveCommitted(QPrivateSignal); //< todo check if we need
    void camerasChanged();
    void isOnlineChanged(bool isOnline, QPrivateSignal);
    void dataNeeded();

protected:
    // These functions must be overridden in derived classes.

    void setFetchInProgress(bool value);

    using FetchCompletionHandler = std::function<void (
        EventSearch::FetchResult result,
        const FetchedDataRanges& fetchedRanges,
        const OptionalTimePeriod& fetchedTimeWindow)>;

    /** Requests next data fetch with a selected parameters. */
    virtual bool requestFetch(
        const FetchRequest& request,
        const FetchCompletionHandler& completionHandler) = 0;

    virtual bool cancelFetch() = 0;

    /** Clear all fetched data. Doesn't need to reset fetched time window. */
    virtual void clearData() = 0;

    template<class Item>
    using ItemCleanupFunction = std::function<void(const Item&)>;

    /** Should be called during every successful fetch to update fetched time window. */
    void setFetchedTimeWindow(const OptionalTimePeriod& value);

    /** Sets whether underlying data store can be populated with new items in live mode. */
    void setLiveSupported(bool value);

    /** Checks whether filter is degenerate and fetch will never return any results. */
    virtual bool isFilterDegenerate() const;

    void onOnlineChanged(bool online);

    int calculateCentralItemIndex(const FetchedDataRanges& ranges,
        const EventSearch::FetchDirection& direction);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
