// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_search_list_model.h"

#include <QtQml/QtQml>

#include <chrono>

#include <client/client_message_processor.h>
#include <client_core/client_core_module.h>
#include <common/common_meta_types.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_manager.h>
#include <utils/common/synctime.h>
#include <utils/common/delayed.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/event_search/models/abstract_search_list_model.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>
#include <nx/utils/model_row_iterator.h>

#include "fetched_data.h"

namespace nx::vms::client::core {

using namespace std::chrono;

namespace {

QString timePeriodToString(const OptionalTimePeriod& period)
{
    return period
        ? QString("from: %1 to: %2").arg(
            nx::utils::timestampToDebugString(period->startTimeMs),
            nx::utils::timestampToDebugString(period->endTimeMs()))
        : "empty";
}

} // namespace

struct AbstractSearchListModel::Private
{
    AbstractSearchListModel* const q;

    int maximumCount = 500; //< Maximum item count.
    bool liveSupported = false; //< Whether underlying data store can be updated in live.
    bool live = false; //< Live mode enabled state.
    bool livePaused = false; //< Live mode paused state.
    bool isOnline = false; //< Whether connection with server is fully established.
    bool offlineAllowed = false; //< Whether the model can work without server connection.
    bool fetchInProgress = false;
    OptionalFetchRequest request;
    OptionalTimePeriod interestTimePeriod; //< Time period of interest
    OptionalTimePeriod fetchedTimeWindow; //< Time window of currently fetched data.

    ManagedCameraSet cameraSet; //< Relevant camera set.

    Private(AbstractSearchListModel* q,
        int maximumCount);

    bool isCameraApplicable(const QnVirtualCameraResourcePtr& camera) const;
};

AbstractSearchListModel::Private::Private(
    AbstractSearchListModel* q,
    int maximumCount)
    :
    q(q),
    maximumCount(maximumCount),
    cameraSet(q->resourcePool(),
        [this](const auto& camera)
        {
            return isCameraApplicable(camera);
        })
{
}

bool AbstractSearchListModel::Private::isCameraApplicable(
    const QnVirtualCameraResourcePtr& camera) const
{
    static const auto kRequiredPermissions = Qn::ReadPermission | Qn::ViewContentPermission;
    const auto user = q->userWatcher()->user();
    return user && camera
        && !camera->flags().testFlag(Qn::desktop_camera)
        && q->resourceAccessManager()->hasPermission(user, camera, kRequiredPermissions);
}

//-------------------------------------------------------------------------------------------------

void AbstractSearchListModel::registerQmlType()
{
    qmlRegisterUncreatableType<AbstractSearchListModel>("nx.vms.client.core", 1, 0,
        "AbstractSearchListModel", "Cannot create instance of AbstractSearchListModel");
}

AbstractSearchListModel::AbstractSearchListModel(
    int maximumCount,
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(systemContext, parent),
    d(new Private(this, maximumCount))
{
    NX_CRITICAL(messageProcessor() && resourcePool());

    const auto watcher = userWatcher();
    connect(watcher, &UserWatcher::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            onOnlineChanged(/*isOnline*/ !user.isNull());
        });

    onOnlineChanged(!userWatcher()->user().isNull());

    connect(&d->cameraSet, &ManagedCameraSet::camerasAboutToBeChanged,
        this, &AbstractSearchListModel::clear);

    connect(&d->cameraSet, &ManagedCameraSet::camerasChanged, this,
        [this]()
        {
            if (isOnline())
                emit camerasChanged();
        });
}

AbstractSearchListModel::AbstractSearchListModel(
    SystemContext* systemContext,
    QObject* parent)
    :
    AbstractSearchListModel(kStandardMaximumItemCount, systemContext, parent)
{
}

AbstractSearchListModel::~AbstractSearchListModel()
{
}

void AbstractSearchListModel::onOnlineChanged(bool isOnline)
{
    if (d->isOnline == isOnline)
        return;

    d->isOnline = isOnline;

    if (d->isOnline)
    {
        emit camerasChanged();
        emit dataNeeded();
    }
    else
    {
        d->cameraSet.setMultipleCameras({});
        clear();
    }

    emit isOnlineChanged(d->isOnline, QPrivateSignal());
}

int AbstractSearchListModel::calculateCentralItemIndex(const FetchedDataRanges& ranges,
    const EventSearch::FetchDirection& direction)
{
    if (direction == EventSearch::FetchDirection::older)
        return  std::max(ranges.body.length - 1, 0);

    const int size = ranges.tail.length + ranges.body.length;
    return ranges.body.length
        ? std::min(ranges.tail.length, size)
        : 0;
}

bool AbstractSearchListModel::fetchInProgress() const
{
    return d->fetchInProgress;
}

bool AbstractSearchListModel::canFetchData(EventSearch::FetchDirection direction) const
{
    if (fetchInProgress()
        || (!d->offlineAllowed && !d->isOnline)
        || isFilterDegenerate()
        || !hasAccessRights())
    {
        return false;
    }

    if (!d->fetchedTimeWindow || !d->interestTimePeriod)
        return true;

    if (direction == EventSearch::FetchDirection::older)
    {
        /** Can fetch older data if there is some gap between fetched and relevant start times.*/
        return d->fetchedTimeWindow->startTimeMs > d->interestTimePeriod->startTimeMs;
    }

    /** Can fetch newer data if there is some gap between fetched and relevant end times.*/
    NX_ASSERT(direction == EventSearch::FetchDirection::newer);
    return d->fetchedTimeWindow->endTimeMs() < d->interestTimePeriod->endTimeMs();
}

void AbstractSearchListModel::setFetchInProgress(bool value)
{
    d->fetchInProgress = value;
}

bool AbstractSearchListModel::fetchData(const FetchRequest& request)
{
    if (isFilterDegenerate() || !canFetchData(request.direction))
        return false;

    setFetchInProgress(true);
    emit fetchCommitStarted(request);
    const bool result = requestFetch(request,
        [this, request](EventSearch::FetchResult result,
            const FetchedDataRanges& ranges,
            const OptionalTimePeriod& timeWindow)
        {
            setFetchInProgress(false);
            setFetchedTimeWindow(timeWindow);
            emit fetchFinished(
                result, calculateCentralItemIndex(ranges, request.direction), request);
        });

    if (!result)
    {
        setFetchInProgress(false);
        emit fetchFinished(EventSearch::FetchResult::failed, -1, request);
    }

    return result;
}

OptionalFetchRequest AbstractSearchListModel::lastFetchRequest() const
{
    return d->request;
}

void AbstractSearchListModel::clear()
{
    NX_VERBOSE(this, "Clear model");

    d->fetchedTimeWindow = {};

    cancelFetch();
    clearData();

    setLive(effectiveLiveSupported());

    const auto emitDataNeeded =
        [this]()
        {
            NX_VERBOSE(this, "Emitting dataNeeded() after a clear");
            emit dataNeeded();
        };

    executeLater(emitDataNeeded, this);
}

bool AbstractSearchListModel::isFilterDegenerate() const
{
    if (!d->interestTimePeriod)
        return false;

    return d->cameraSet.type() != ManagedCameraSet::Type::all
        && d->cameraSet.cameras().empty();
}

const ManagedCameraSet& AbstractSearchListModel::cameraSet() const
{
    return d->cameraSet;
}

ManagedCameraSet& AbstractSearchListModel::cameraSet()
{
    return d->cameraSet;
}

bool AbstractSearchListModel::isOnline() const
{
    return d->isOnline;
}

bool AbstractSearchListModel::cancelFetch()
{
    return false;
}

bool AbstractSearchListModel::isConstrained() const
{
    return d->interestTimePeriod != QnTimePeriod::anytime();
}

bool AbstractSearchListModel::hasAccessRights() const
{
    return true;
}

TextFilterSetup* AbstractSearchListModel::textFilter() const
{
    return nullptr;
}

std::pair<int, int> AbstractSearchListModel::find(milliseconds timestamp) const
{
    if (!d->fetchedTimeWindow || !d->fetchedTimeWindow->contains(timestamp))
        return {};

    const auto range = std::equal_range(
        nx::utils::ModelRowIterator::cbegin(TimestampRole, this),
        nx::utils::ModelRowIterator::cend(TimestampRole, this),
        QVariant::fromValue(microseconds(timestamp)),
        [](const QVariant& left, const QVariant& right)
        {
            return right.value<microseconds>() < left.value<microseconds>();
        });

    return {range.first.row(), range.second.row()};
}

int AbstractSearchListModel::maximumCount() const
{
    return d->maximumCount;
}

FetchRequest AbstractSearchListModel::requestForDirection(
    EventSearch::FetchDirection direction) const
{
    const auto centralPointUs =
        [this, direction]()
        {
            if (!d->fetchedTimeWindow)
                return qnSyncTime->currentTimePoint();

            return duration_cast<microseconds>(direction == EventSearch::FetchDirection::older
                ? d->fetchedTimeWindow->startTime()
                : d->fetchedTimeWindow->endTime());
        }();

    return {.direction = direction, .centralPointUs = centralPointUs};
}

OptionalTimePeriod AbstractSearchListModel::interestTimePeriod() const
{
    return d->interestTimePeriod;
}

void AbstractSearchListModel::setInterestTimePeriod(const OptionalTimePeriod& value)
{
    if (value == d->interestTimePeriod)
        return;

    d->interestTimePeriod = value;

    if (!d->interestTimePeriod
        || !d->interestTimePeriod->isValid()
        || !d->interestTimePeriod->isInfinite())
    {
        clear();
    }

    cancelFetch();

    if (!d->interestTimePeriod || d->interestTimePeriod->isInfinite())
        setLive(false);

    emit dataNeeded();
}

bool AbstractSearchListModel::liveSupported() const
{
    return d->liveSupported;
}

bool AbstractSearchListModel::effectiveLiveSupported() const
{
    return liveSupported() && (d->interestTimePeriod && d->interestTimePeriod->isInfinite());
}

void AbstractSearchListModel::setLiveSupported(bool value)
{
    if (d->liveSupported == value)
        return;

    d->liveSupported = value;
    setLive(d->live && d->liveSupported);

    clear();
}

bool AbstractSearchListModel::isLive() const
{
    return d->liveSupported && d->live;
}

void AbstractSearchListModel::setLive(bool value)
{
    if (value == d->live)
        return;

    if (value && !(NX_ASSERT(liveSupported()) && effectiveLiveSupported()))
        return;

    NX_VERBOSE(this, "Setting live mode to %1", value);
    d->live = value;

    emit liveChanged(d->live, QPrivateSignal());
}

bool AbstractSearchListModel::livePaused() const
{
    return d->livePaused;
}

void AbstractSearchListModel::setLivePaused(bool value)
{
    if (d->livePaused == value)
        return;

    NX_VERBOSE(this, "Setting live %1", (value ? "paused" : "unpaused"));
    d->livePaused = value;

    emit livePausedChanged(d->livePaused, QPrivateSignal());
}

bool AbstractSearchListModel::offlineAllowed() const
{
    return d->offlineAllowed;
}

void AbstractSearchListModel::setOfflineAllowed(bool value)
{
    if (d->offlineAllowed == value)
        return;

    NX_VERBOSE(this, "Setting offline allowed to %1", value);
    d->offlineAllowed = value;
}

OptionalTimePeriod AbstractSearchListModel::fetchedTimeWindow() const
{
    return d->fetchedTimeWindow;
}

void AbstractSearchListModel::setFetchedTimeWindow(const OptionalTimePeriod& value)
{
    NX_VERBOSE(this, "Set fetched time window:\n %1", timePeriodToString(value));
    d->fetchedTimeWindow = value;
}

} // namespace nx::vms::client::core
