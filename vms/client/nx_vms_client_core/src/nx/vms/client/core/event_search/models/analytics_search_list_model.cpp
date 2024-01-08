// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_list_model.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QPixmap>
#include <QtQml/QQmlEngine>

#include <analytics/db/analytics_db_types.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/object_type_dictionary.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/api/data/thumbnail_filter.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/event_search/utils/live_analytics_receiver.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/text/human_readable.h>
#include <storage/server_storage_manager.h>

#include "detail/analytic_data_storage.h"
#include "detail/object_track_facade.h"

namespace nx::vms::client::core {

namespace {

using namespace std::chrono;
using namespace nx::analytics::db;

using FetchResult = EventSearch::FetchResult;
using FetchDirection = EventSearch::FetchDirection;
using Facade = detail::ObjectTrackFacade;
using Predicate = event_search::detail::Predicate<Facade>;

static constexpr milliseconds kMetadataTimerInterval = 1000ms;
static constexpr milliseconds kDataChangedInterval = 500ms;

using GetCallback = std::function<void(bool, rest::Handle, nx::analytics::db::LookupResult&&)>;
using ObjectTrackFetchedData = FetchedData<nx::analytics::db::LookupResult>;

microseconds objectDuration(const ObjectTrack& track)
{
    // TODO: #vkutin Is this duration formula good enough for us?
    // Or we need to add some "lastAppearanceDurationUsec"?
    return microseconds(track.lastAppearanceTimeUs - track.firstAppearanceTimeUs);
}

template<class Set, class Hash>
void subtractKeysFromSet(Set& minuend, const Hash& subtrahend)
{
    for (const auto& [key, value]: nx::utils::constKeyValueRange(subtrahend))
        minuend.remove(key);
}

/**
 * Merges old data (which can't be changed) and new one to one container. Removes intersected
 * elements. Source container is always sorted in descending order.
 */
template<typename Facade,
    typename NewDataContainer,
    typename OldDataContainer>
void mergeOldData(NewDataContainer& newData,
    const OldDataContainer& oldData,
    FetchDirection direction)
{
    if (oldData.empty())
        return; // Nothing to merge.

    const auto centralTimePoint = newData.empty()
        ? std::chrono::microseconds(0)
        : Facade::startTime(newData.front());

    if (direction == FetchDirection::newer)
        newData.insert(newData.begin(), oldData.rbegin(), oldData.rend());
    else
        newData.insert(newData.begin(), oldData.begin(), oldData.end());

    if (newData.empty())
        return;

    const auto startIt = std::lower_bound(newData.begin(), newData.end(), centralTimePoint,
        Predicate::lowerBound(direction));
    const auto endIt = std::upper_bound(startIt, newData.end(), centralTimePoint,
        Predicate::upperBound(direction));
    const auto removeStartIt = std::unique(startIt, endIt,
        [](const auto& left, const auto& right)
        {
            return Facade::id(left) == Facade::id(right);
        });

    newData.erase(removeStartIt, endIt);
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct AnalyticsSearchListModel::Private: public QObject
{
    AnalyticsSearchListModel* const q;

    const std::unique_ptr<TextFilterSetup> textFilter;

    rest::Handle currentHttpRequestId = 0;

    QnUuid selectedEngine;
    LiveProcessingMode liveProcessingMode = LiveProcessingMode::automaticAdd;

    QRectF filterRect;
    QStringList attributeFilters;
    QStringList selectedObjectTypes;
    std::set<QString> relevantObjectTypes;
    mutable QHash<QString, bool> objectTypeAcceptanceCache;

    const QScopedPointer<nx::utils::PendingOperation> emitDataChanged;
    bool liveReceptionActive = false;

    using MetadataReceiverList = std::vector<std::unique_ptr<LiveAnalyticsReceiver>>;
    MetadataReceiverList metadataReceivers;
    QTimer metadataProcessingTimer;

    QSet<QnUuid> externalBestShotTracks;
    QSet<QnUuid> dataChangedTrackIds; //< For which tracks delayed dataChanged is queued.

    bool newTrackCountUnknown = false;
    bool gapBeforeNewTracks = false;
    bool keepLiveDataAtClear = false;

    detail::AnalyticDataStorage newTracks;
    detail::AnalyticDataStorage noBestShotTracks;
    detail::AnalyticDataStorage data;

    LiveTimestampGetter liveTimestampGetter;

    using MetadataByCamera =
        QHash<QnVirtualCameraResourcePtr, QList<QnAbstractCompressedMetadataPtr>>;
    /**
     * Temporary storage for the data which can't be applied now becuase view stream data from the
     * camera is late.
     */
    MetadataByCamera deferredMetadata;


    Private(AnalyticsSearchListModel* q);

    void emitDataChangedIfNeeded();
    void advanceTrack(nx::analytics::db::ObjectTrack& track,
        nx::analytics::db::ObjectPosition&& position, bool emitDataChanged = true);

    void updateRelevantObjectTypes();

    const nx::analytics::taxonomy::AbstractObjectType*
        objectTypeById(const QString& objectTypeId) const;

    QString iconPath(const QString& objectTypeId) const;

    QString description(const ObjectTrack& track) const;

    QString engineName(const ObjectTrack& track) const;

    rest::Handle getObjects(
        const FetchRequest& request,
        int limit,
        GetCallback callback) const;

    rest::Handle lookupObjectTracksCached(
        const nx::analytics::db::Filter& request,
        GetCallback callback) const;

    bool canViewArchive(const QnVirtualCameraResourcePtr& camera) const;

    nx::utils::Guard makeAvailableNewTracksGuard();

    void updateMetadataReceivers();

    void processMetadata();

    using CleanupFunction = AbstractSearchListModel::ItemCleanupFunction<
        nx::analytics::db::ObjectTrack>;
    CleanupFunction itemCleanupFunction(
        detail::AnalyticDataStorage& storage);

    void setLiveReceptionActive(bool value);

    bool isAcceptedObjectType(const QString& typeId) const;

    FetchedDataRanges applyFetchedData(LookupResult&& data, const FetchRequest& request);
};

AnalyticsSearchListModel::Private::Private(AnalyticsSearchListModel* q):
    q(q),
    textFilter(std::make_unique<TextFilterSetup>()),
    emitDataChanged(new nx::utils::PendingOperation(
        [this] { emitDataChangedIfNeeded(); }, kDataChangedInterval, q))
{
    emitDataChanged->setFlags(nx::utils::PendingOperation::NoFlags);

    metadataProcessingTimer.setInterval(kMetadataTimerInterval.count());
    connect(&metadataProcessingTimer, &QTimer::timeout, this, &Private::processMetadata);
    metadataProcessingTimer.start();

    connect(textFilter.get(), &TextFilterSetup::textChanged, this,
        [this]()
        {
            this->q->clear();
            emit this->q->combinedTextFilterChanged();
        });

    connect(q, &AnalyticsSearchListModel::attributeFiltersChanged,
        q, &AnalyticsSearchListModel::combinedTextFilterChanged);

    connect(q, &AbstractSearchListModel::camerasChanged, this, &Private::updateMetadataReceivers);

    connect(q, &AbstractSearchListModel::fetchFinished, this,
        [this](FetchResult result, int /*centralItemIndex*/,  FetchRequest request)
        {
            if (result == FetchResult::complete && request.direction == FetchDirection::newer)
                gapBeforeNewTracks = false;
        });

    connect(q->systemContext()->accessController(), &AccessController::permissionsMaybeChanged, this,
        [this](const QnResourceList& resources)
        {
            const auto isCamera =
                [](const QnResourcePtr& resource)
            {
                return resource.objectCast<QnVirtualCameraResource>();
            };

            if (resources.isEmpty() || std::any_of(resources.begin(), resources.end(), isCamera))
                updateMetadataReceivers();
        });

    const auto cameraStatusListener =
        new core::SessionResourcesSignalListener<QnVirtualCameraResource>(q->systemContext(), q);

    cameraStatusListener->addOnSignalHandler(
        &QnResource::statusChanged,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            if (NX_ASSERT(this->q->isOnline()) && !camera->enabledAnalyticsEngines().empty())
                updateMetadataReceivers();
        });

    cameraStatusListener->addOnPropertyChangedHandler(
        [this](const QnVirtualCameraResourcePtr& camera, const QString& key)
        {
            if (!NX_ASSERT(this->q->isOnline()) || !camera->isOnline())
                return;

            if (key == QnVirtualCameraResource::kCompatibleAnalyticsEnginesProperty
                || key == QnVirtualCameraResource::kUserEnabledAnalyticsEnginesProperty)
            {
                updateMetadataReceivers();
            }
        });

    cameraStatusListener->start();

    const auto storageManager = q->systemContext()->serverStorageManager();
    connect(storageManager, &QnServerStorageManager::activeMetadataStorageChanged, this,
        [this](const QnMediaServerResourcePtr& server)
        {
            const auto relevantCameras = this->q->cameraSet().cameras();
            const auto serverFootageCameras =
                this->q->systemContext()->cameraHistoryPool()->getServerFootageCameras(server);

            for (const auto& camera: serverFootageCameras)
            {
                if (relevantCameras.contains(camera.objectCast<QnVirtualCameraResource>()))
                {
                    this->q->clear();
                    break;
                }
            }
        });

    const auto watcher = q->systemContext()->analyticsTaxonomyStateWatcher();
    if (NX_ASSERT(watcher))
    {
        connect(watcher, &nx::analytics::taxonomy::AbstractStateWatcher::stateChanged, this,
            [this]()
            {
                objectTypeAcceptanceCache.clear();
                updateRelevantObjectTypes();
            });
    }
}

void AnalyticsSearchListModel::Private::emitDataChangedIfNeeded()
{
    if (dataChangedTrackIds.empty())
        return;

    for (const auto& id: dataChangedTrackIds)
    {
        const auto index = data.indexOf(id);
        if (index < 0)
            continue;

        const auto modelIndex = q->index(index);
        emit q->dataChanged(modelIndex, modelIndex);
    }

    dataChangedTrackIds.clear();
}

void AnalyticsSearchListModel::Private::advanceTrack(ObjectTrack& track,
    ObjectPosition&& position, bool emitDataChanged)
{
    using nx::common::metadata::AttributeEx;
    using nx::common::metadata::NumericRange;

    for (const auto& attribute: position.attributes)
    {
        const auto foundIt = std::find_if(track.attributes.begin(), track.attributes.end(),
            [&attribute](const nx::common::metadata::Attribute& value)
            {
                return attribute.name == value.name;
            });

        if (foundIt != track.attributes.end())
        {
            AttributeEx numericAttribute(attribute);
            if (std::holds_alternative<NumericRange>(numericAttribute.value))
            {
                const auto range = AttributeEx::parseRangeFromValue(foundIt->value);
                if (range.from.has_value() && range.to.has_value())
                {
                    numericAttribute.addRange(range);
                    *foundIt = {numericAttribute.name, numericAttribute.stringValue()};
                    continue;
                }
            }
        }
        addAttributeIfNotExists(&track.attributes, attribute);
    }

    track.lastAppearanceTimeUs = position.timestampUs;

    if (emitDataChanged)
    {
        dataChangedTrackIds.insert(track.id);
        this->emitDataChanged->requestOperation();
    }
}

void AnalyticsSearchListModel::Private::updateRelevantObjectTypes()
{
    const std::set<QString> value(selectedObjectTypes.begin(), selectedObjectTypes.end());

    if (value == relevantObjectTypes)
        return;

    relevantObjectTypes = value;
    emit q->relevantObjectTypesChanged();
}

const nx::analytics::taxonomy::AbstractObjectType*
    AnalyticsSearchListModel::Private::objectTypeById(const QString& objectTypeId) const
{
    const auto watcher = q->systemContext()->analyticsTaxonomyStateWatcher();
    if (!NX_ASSERT(watcher))
        return nullptr;

    const auto state = watcher->state();
    return state ? state->objectTypeById(objectTypeId) : nullptr;
}

QString AnalyticsSearchListModel::Private::iconPath(const QString& objectTypeId) const
{
    const auto iconManager = analytics::IconManager::instance();
    const auto objectType = objectTypeById(objectTypeId);

    return objectType
        ? iconManager->iconPath(objectType->icon())
        : iconManager->fallbackIconPath();
}

QString AnalyticsSearchListModel::Private::description(
    const ObjectTrack& track) const
{
    if (!ini().showDebugTimeInformationInEventSearchData)
        return QString();

    // TODO: #sivanov Actualize used system context.
    const auto timeWatcher = q->serverTimeWatcher();
    const auto start = timeWatcher->displayTime(
        duration_cast<milliseconds>((Facade::startTime(track))).count());
    const auto duration = objectDuration(track);

    // Not translatable, debug string.
    return nx::format(
        "Timestamp: %1 us<br>%2<br>"
        "Duration: %3 us<br>%4<br>"
        "Best shot: %5 us<br>%6<br>"
        "Track ID: %7<br>").args(
            track.firstAppearanceTimeUs,
            start.toString(Qt::RFC2822Date),
            duration.count(),
            text::HumanReadable::timeSpan(duration_cast<milliseconds>(duration)),
            track.bestShot.timestampUs,
            timeWatcher->displayTime(track.bestShot.timestampUs / 1000).toString(Qt::RFC2822Date),
            track.id.toSimpleString());
}

QString AnalyticsSearchListModel::Private::engineName(
    const ObjectTrack& track) const
{
    using nx::analytics::taxonomy::AbstractState;

    if (track.analyticsEngineId.isNull())
        return {};

    const std::shared_ptr<AbstractState> taxonomyState =
        q->systemContext()->analyticsTaxonomyState();

    if (!taxonomyState)
        return {};

    if (const auto engine = taxonomyState->engineById(track.analyticsEngineId.toString()))
        return engine->name();

    return {};
}

rest::Handle AnalyticsSearchListModel::Private::getObjects(
    const FetchRequest& request,
    int limit,
    GetCallback callback) const
{
    if (!NX_ASSERT(callback && q->connection() && !q->isFilterDegenerate()))
        return {};

    Filter filter;
    if (q->cameraSet().type() != ManagedCameraSet::Type::all)
    {
        for (const auto& camera: q->cameraSet().cameras())
            filter.deviceIds.insert(camera->getId());
    }

    filter.maxObjectTracksToSelect = limit;
    filter.freeText = q->combinedTextFilter();
    filter.analyticsEngineId = selectedEngine;
    filter.sortOrder = EventSearch::sortOrderFromDirection(request.direction);
    filter.timePeriod = request.period(q->interestTimePeriod());

    if (!selectedObjectTypes.isEmpty())
        filter.objectTypeId = std::set(selectedObjectTypes.begin(), selectedObjectTypes.end());

    if (q->cameraSet().type() == ManagedCameraSet::Type::single && filterRect.isValid())
        filter.boundingBox = filterRect;

    NX_VERBOSE(q, "Requesting object tracks:\n    from: %1\n    to: %2\n"
        "    box: %3\n    text filter: %4\n    sort: %5\n    limit: %6",
        nx::utils::timestampToDebugString(filter.timePeriod.startTimeMs),
        nx::utils::timestampToDebugString(filter.timePeriod.endTimeMs()),
        filter.boundingBox,
        filter.freeText,
        QVariant::fromValue(filter.sortOrder).toString(),
        filter.maxObjectTracksToSelect);

    return lookupObjectTracksCached(filter, nx::utils::guarded(q, callback));
}

rest::Handle AnalyticsSearchListModel::Private::lookupObjectTracksCached(
    const nx::analytics::db::Filter& request,
    GetCallback callback) const
{
    if (!NX_ASSERT(q->thread() == QThread::currentThread()))
        return {};

    struct LookupContext
    {
        nx::analytics::db::Filter filter;
        rest::Handle handle{};
        std::map<intptr_t, GetCallback> callbacks;
    };
    static std::vector<LookupContext> cachedRequests;

    NX_VERBOSE(q, "Called lookupObjectTracksCached(), %1 requests are currently cached",
        cachedRequests.size());

    const auto contextIt = std::find_if(cachedRequests.begin(), cachedRequests.end(),
        [request](const LookupContext& context) { return context.filter == request; });

    if (contextIt != cachedRequests.end())
    {
        NX_VERBOSE(q, "Using cached request, handle=%1", contextIt->handle);
        // One model can have only one request running at any time.
        // Placing second request means the previous one was cancelled.
        contextIt->callbacks[(intptr_t) this] = std::move(callback);
        return contextIt->handle;
    }
    else
    {
        const auto responseHandler = nx::utils::guarded(q,
            [this](bool success, rest::Handle handle, nx::analytics::db::LookupResult&& result)
            {
                const auto contextIt = std::find_if(cachedRequests.begin(), cachedRequests.end(),
                    [&](const LookupContext& context) { return context.handle == handle; });

                NX_VERBOSE(q, "Received a response, handle=%1", handle);

                if (contextIt == cachedRequests.end())
                {
                    NX_VERBOSE(q, "No corresponding cached request found (was cancelled)");
                    return;
                }

                NX_VERBOSE(q, "Found cached request with %1 callbacks", contextIt->callbacks.size());
                if (NX_ASSERT(contextIt->handle == handle && !contextIt->callbacks.empty()))
                {
                    const auto last = --contextIt->callbacks.end();

                    for (auto it = contextIt->callbacks.begin(); it != last; ++it)
                        (it->second)(success, handle, nx::analytics::db::LookupResult(result));

                    (last->second)(success, handle, std::move(result));
                }

                cachedRequests.erase(contextIt);

                NX_VERBOSE(q, "Removed handle=%1 from cache, %2 requests are still cached",
                    handle, cachedRequests.size());
        });

        LookupContext context;
        context.filter = request;
        context.callbacks[(intptr_t) this] = std::move(callback);

        const auto api = q->connectedServerApi();
        if (!api)
            return {};

        context.handle = api->lookupObjectTracks(
            request, /*isLocal*/ false, responseHandler, q->thread());

        NX_VERBOSE(q, "Created a new request, handle=%1", context.handle);
        cachedRequests.push_back(context);
        return context.handle;
    }
}

bool AnalyticsSearchListModel::Private::canViewArchive(
    const QnVirtualCameraResourcePtr& camera) const
{
    return q->accessController()->hasPermissions(camera, Qn::ViewFootagePermission);
}

nx::utils::Guard AnalyticsSearchListModel::Private::makeAvailableNewTracksGuard()
{
    return nx::utils::Guard(
        [this, oldValue = q->availableNewTracks()]()
        {
            if (oldValue != q->availableNewTracks())
                emit q->availableNewTracksChanged();
        });
}

void AnalyticsSearchListModel::Private::updateMetadataReceivers()
{
    if (liveReceptionActive)
    {
        auto cameras = q->cameraSet().cameras();
        MetadataReceiverList newMetadataReceivers;

        // Remove no longer relevant deferred metadata.
        const auto deferredCameras = deferredMetadata.keys();
        for (const auto& camera: deferredCameras)
        {
            if (!cameras.contains(camera))
                deferredMetadata.remove(camera);
        }

        const auto isReceiverRequired =
            [this](const QnVirtualCameraResourcePtr& camera)
        {
            return camera->isOnline()
                && !camera->enabledAnalyticsEngines().empty()
                && q->systemContext()->accessController()->hasPermissions(
                    camera, Qn::ViewFootagePermission);
        };

        // Preserve existing receivers that are still relevant.
        for (auto& receiver: metadataReceivers)
        {
            if (cameras.remove(receiver->camera()) && isReceiverRequired(receiver->camera()))
                newMetadataReceivers.emplace_back(receiver.release());
        }

        // Create new receivers if needed.
        for (const auto& camera: cameras)
        {
            if (!isReceiverRequired(camera))
                continue;

            newMetadataReceivers.emplace_back(new LiveAnalyticsReceiver(camera));

            connect(newMetadataReceivers.back().get(), &LiveAnalyticsReceiver::dataOverflow,
                this, &Private::processMetadata);
        }

        NX_VERBOSE(q, "Ensured metadata receivers for %1 cameras", newMetadataReceivers.size());
        metadataReceivers = std::move(newMetadataReceivers);
    }
    else
    {
        metadataReceivers.clear();
        deferredMetadata.clear();
        NX_VERBOSE(q, "Released all metadata receivers");
    }
}

void AnalyticsSearchListModel::Private::processMetadata()
{
    // Don't start receiving live data until first archive fetch is finished.
    if (data.empty() && (q->fetchInProgress() || q->canFetchData(FetchDirection::older)))
        return;

    // Special handling if live mode is paused (analytics tab is hidden).
    if (q->livePaused() && !data.empty())
    {
        gapBeforeNewTracks = true;

        if (liveProcessingMode == LiveProcessingMode::automaticAdd)
        {
            // Completely stop metadata processing in automatic mode.
            q->setLive(false);
        }
        else
        {
            const auto availableNewTracksGuard = makeAvailableNewTracksGuard();
            newTrackCountUnknown = newTrackCountUnknown || !newTracks.empty();

            // Discard previously received tracks in manual mode.
            subtractKeysFromSet(/*minuend*/ externalBestShotTracks,
                /*subtrahend*/ newTracks.idToTimestamp);
            subtractKeysFromSet(/*minuend*/ externalBestShotTracks,
                /*subtrahend*/ noBestShotTracks.idToTimestamp);
            newTracks.clear();
            noBestShotTracks.clear();
        }
    }

    setLiveReceptionActive(
        (q->isLive() || liveProcessingMode == LiveProcessingMode::manualAdd)
        && (!q->interestTimePeriod() || q->interestTimePeriod()->isInfinite()) //< For manualAdd mode.
        && q->isOnline()
        && !q->livePaused()
        && (!q->isFilterDegenerate() || q->hasOnlyLiveCameras()));

    if (!liveReceptionActive)
        return;

    // Fetch all metadata packets from receiver buffers.

    std::vector<QnAbstractCompressedMetadataPtr> metadataPackets;
    int numSources = 0;

    static constexpr auto kMaxTimestamp = std::numeric_limits<milliseconds>::max();

    milliseconds liveTimestamp{};
    if (liveTimestampGetter && ini().delayRightPanelLiveAnalytics)
    {
        // Choose "live" of a camera with the longest delay.
        for (const auto& receiver: metadataReceivers)
        {
            const auto timestamp = liveTimestampGetter(receiver->camera());
            if (timestamp > 0ms && timestamp != kMaxTimestamp)
                liveTimestamp = std::max(liveTimestamp, timestamp);
        }
    }

    MetadataByCamera currentMetadata;
    currentMetadata.swap(deferredMetadata);

    // Adds all new availble packages to the temporary metadata.
    for (const auto& receiver: metadataReceivers)
        currentMetadata[receiver->camera()].append(receiver->takeData());

    for (auto&& [camera, packets]: nx::utils::keyValueRange(currentMetadata))
    {
        if (packets.empty())
            continue;

        const auto metadataTimestamp =
            [](const QnAbstractCompressedMetadataPtr& packet) -> milliseconds
            {
                return packet->isSpecialTimeValue()
                    ? kMaxTimestamp
                    : duration_cast<milliseconds>(microseconds(packet->timestamp));
            };

        // TODO: #vkutin Normally packets should be sorted except maybe best shot packets.
        // Try to get rid of this additional sort in the future.
        std::stable_sort(packets.begin(), packets.end(),
            [metadataTimestamp](const auto& left, const auto& right)
            {
                return metadataTimestamp(left) < metadataTimestamp(right);
            });

        if (liveTimestamp > 0ms)
        {
            const auto pos = std::lower_bound(packets.begin(), packets.end(), liveTimestamp,
                [metadataTimestamp](const auto& left, milliseconds right)
                {
                    return metadataTimestamp(left) < right;
                });

            if (pos == packets.begin())
            {
                deferredMetadata[camera].swap(packets);
            }
            else if (pos != packets.end())
            {
                deferredMetadata[camera] = packets.mid(pos - packets.begin());
                packets.erase(pos, packets.end());
            }
        }

        metadataPackets.insert(metadataPackets.end(),
            std::make_move_iterator(packets.begin()),
            std::make_move_iterator(packets.end()));

        ++numSources;
    }

    if (metadataPackets.empty())
        return;

    // Process all metadata packets.

    NX_VERBOSE(q, "Processing %1 live metadata packets from %2 sources",
        (int) metadataPackets.size(), numSources);

    struct FoundObjectTrack
    {
        detail::AnalyticDataStorage* storage = nullptr;
        int index = -1;
    };

    const auto findObject =
        [this](const QnUuid& trackId) -> FoundObjectTrack
    {
        int index = newTracks.indexOf(trackId);
        if (index >= 0)
            return {&newTracks, index};

        index = data.indexOf(trackId);
        if (index >= 0)
            return {&data, index};

        index = noBestShotTracks.indexOf(trackId);
        if (index >= 0)
            return {&noBestShotTracks, index};

        return {};
    };

    nx::analytics::db::Filter filter;
    filter.freeText = q->combinedTextFilter();
    filter.objectTypeId = relevantObjectTypes;
    filter.analyticsEngineId = selectedEngine;

    if (filterRect.isValid())
        filter.boundingBox = filterRect;

    const nx::analytics::taxonomy::ObjectTypeDictionary objectTypeDictionary(
        q->systemContext()->analyticsTaxonomyStateWatcher());

    const int oldNewTrackCount = newTracks.size();

    for (const auto& metadata: metadataPackets)
    {
        NX_ASSERT(metadata->metadataType == MetadataType::ObjectDetection);
        const auto compressedMetadata =
            std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
        const auto objectMetadata =
            nx::common::metadata::fromCompressedMetadataPacket(compressedMetadata);

        if (!objectMetadata || objectMetadata->objectMetadataList.empty())
            continue;

        for (const auto& item: objectMetadata->objectMetadataList)
        {
            auto found = findObject(item.trackId);
            if (item.isBestShot())
            {
                if (!found.storage)
                    continue; //< A valid situation - a track can be filtered out.

                if (item.objectMetadataType
                    == nx::common::metadata::ObjectMetadataType::externalBestShot)
                {
                    externalBestShotTracks.insert(item.trackId);
                }

                auto& track = found.storage->items[found.index];
                track.bestShot.timestampUs = objectMetadata->timestampUs;
                track.bestShot.rect = item.boundingBox;
                track.bestShot.streamIndex = objectMetadata->streamIndex;

                if (found.storage == &data)
                {
                    // If it was an already loaded track, emit dataChanged.
                    dataChangedTrackIds.insert(track.id);
                    emitDataChanged->requestOperation();
                }
                else if (found.storage == &noBestShotTracks)
                {
                    // If it was a no-best-shot track, move it to the newly added.
                    const int index = newTracks.insert(noBestShotTracks.take(found.index));
                    found = FoundObjectTrack{&newTracks, index};
                }
            }

            ObjectPosition pos;
            pos.deviceId = objectMetadata->deviceId;
            pos.timestampUs = objectMetadata->timestampUs;
            pos.durationUs = objectMetadata->durationUs;
            pos.boundingBox = item.boundingBox;

            if (found.storage)
            {
                auto& track = found.storage->items[found.index];
                if (!item.isBestShot())
                    track.objectTypeId = item.typeId;
                pos.attributes = item.attributes;
                advanceTrack(track, std::move(pos), /*emitDataChanged*/ found.storage == &data);
                continue;
            }

            if (!isAcceptedObjectType(item.typeId))
                continue;

            ObjectTrack newTrack;
            newTrack.id = item.trackId;
            newTrack.deviceId = objectMetadata->deviceId;
            newTrack.objectTypeId = item.typeId;
            newTrack.analyticsEngineId = item.analyticsEngineId;
            newTrack.attributes = item.attributes;
            newTrack.firstAppearanceTimeUs = objectMetadata->timestampUs;
            newTrack.lastAppearanceTimeUs = objectMetadata->timestampUs;
            newTrack.objectPosition.add(item.boundingBox);

            if (!filter.acceptsTrack(newTrack, objectTypeDictionary))
                continue;

            if (newTrack.bestShot.initialized())
                newTracks.insert(std::move(newTrack));
            else
                noBestShotTracks.insert(std::move(newTrack));
        }
    }

    // Limit the size of the no-best-shot track queue.
    if (noBestShotTracks.size() > q->maximumCount())
    {
        const auto removeBegin = noBestShotTracks.items.begin() + q->maximumCount();
        const auto cleanupFunction = itemCleanupFunction(noBestShotTracks);
        std::for_each(removeBegin, noBestShotTracks.items.end(), cleanupFunction);
        noBestShotTracks.items.erase(removeBegin, noBestShotTracks.items.end());
    }

    // Limit the size of the available new tracks queue.
    if (newTracks.size() > q->maximumCount())
    {
        const auto removeBegin = newTracks.items.begin() + q->maximumCount();
        const auto cleanupFunction = itemCleanupFunction(newTracks);
        std::for_each(removeBegin, newTracks.items.end(), cleanupFunction);
        newTracks.items.erase(removeBegin, newTracks.items.end());
        gapBeforeNewTracks = true;
    }

    if (newTracks.size() == oldNewTrackCount)
        return;

    NX_VERBOSE(q, "Detected %1 new object tracks", newTracks.size() - oldNewTrackCount);

    if (liveProcessingMode == LiveProcessingMode::manualAdd)
        emit q->availableNewTracksChanged();
    else
        q->commitAvailableNewTracks();
}

AnalyticsSearchListModel::Private::CleanupFunction
    AnalyticsSearchListModel::Private::itemCleanupFunction(detail::AnalyticDataStorage& storage)
{
    const auto cleanupFunction =
        [this, &storage](const nx::analytics::db::ObjectTrack& track)
        {
            storage.idToTimestamp.remove(track.id);
            externalBestShotTracks.remove(track.id);
        };

    return cleanupFunction;
}

void AnalyticsSearchListModel::Private::setLiveReceptionActive(bool value)
{
    if (liveReceptionActive == value)
        return;

    NX_VERBOSE(q, "Setting live reception %1", (value ? "active" : "inactive"));
    liveReceptionActive = value;

    updateMetadataReceivers();
}

bool AnalyticsSearchListModel::Private::isAcceptedObjectType(const QString& typeId) const
{
    if (!objectTypeAcceptanceCache.contains(typeId))
    {
        const auto objectType = objectTypeById(typeId);
        objectTypeAcceptanceCache[typeId] =
            objectType && !objectType->isLiveOnly() && !objectType->isNonIndexable();
    }

    return objectTypeAcceptanceCache.value(typeId);
}

FetchedDataRanges AnalyticsSearchListModel::Private::applyFetchedData(
    LookupResult&& newData,
    const FetchRequest& request)
{
    // TODO: ynikitenkov: optimize this part. We could store some wide window of data
    // and slide inside it until we need some new data.
    mergeOldData<Facade>(newData, data.items, request.direction);

    auto fetched = makeFetchedData<Facade>(data.items, newData, request);
    truncateFetchedData(fetched, request.direction, q->maximumCount());

    updateEventSearchData<Facade>(q, data.items, fetched, request.direction);

    // Explicitly update fetched time window in case of new tracks update.
    q->setFetchedTimeWindow(timeWindow<Facade>(data.items));

    return fetched.ranges;
}

//-------------------------------------------------------------------------------------------------

AnalyticsSearchListModel::AnalyticsSearchListModel(
    int maximumCount,
    SystemContext* context,
    QObject* parent)
    :
    base_type(maximumCount, context, parent),
    d(new Private(this))
{
    setLiveSupported(true);
}

AnalyticsSearchListModel::AnalyticsSearchListModel(
    SystemContext* context,
    QObject* parent)
    :
    AnalyticsSearchListModel(kStandardMaximumItemCount, context, parent)
{
}

AnalyticsSearchListModel::~AnalyticsSearchListModel()
{
}

QRectF AnalyticsSearchListModel::filterRect() const
{
    return d->filterRect;
}

void AnalyticsSearchListModel::setFilterRect(const QRectF& relativeRect)
{
    if (d->filterRect == relativeRect)
        return;

    clear();
    d->filterRect = relativeRect;
    NX_VERBOSE(this, "Set filter rect to \"%1\"", d->filterRect);
    emit filterRectChanged();
}

TextFilterSetup* AnalyticsSearchListModel::textFilter() const
{
    return d->textFilter.get();
}

QnUuid AnalyticsSearchListModel::selectedEngine() const
{
    return d->selectedEngine;
}

void AnalyticsSearchListModel::setSelectedEngine(const QnUuid& value)
{
    if (d->selectedEngine == value)
        return;

    clear();
    d->selectedEngine = value;
    NX_VERBOSE(this, "Set selected engine to %1", d->selectedEngine.toString());
    emit selectedEngineChanged();
}

QStringList AnalyticsSearchListModel::selectedObjectTypes() const
{
    return d->selectedObjectTypes;
}

void AnalyticsSearchListModel::setSelectedObjectTypes(const QStringList& value)
{
    if (d->selectedObjectTypes == value)
        return;

    clear();
    d->selectedObjectTypes = value;
    NX_VERBOSE(this, "Set selected object type to \"%1\"", d->selectedObjectTypes);
    emit selectedObjectTypesChanged();

    d->updateRelevantObjectTypes();
}

const std::set<QString>& AnalyticsSearchListModel::relevantObjectTypes() const
{
    return d->relevantObjectTypes;
}

QStringList AnalyticsSearchListModel::attributeFilters() const
{
    return d->attributeFilters;
}

void AnalyticsSearchListModel::setAttributeFilters(const QStringList& value)
{
    if (d->attributeFilters == value)
        return;

    clear();
    d->attributeFilters = value;
    NX_VERBOSE(this, "Set attribute filters to \"%1\"", d->attributeFilters.join(" "));
    emit attributeFiltersChanged();
}

QString AnalyticsSearchListModel::combinedTextFilter() const
{
    const auto freeText = NX_ASSERT(textFilter())
        ? textFilter()->text()
        : QString{};

    const auto attributesText = attributeFilters().join(" ");
    if (attributesText.isEmpty())
        return freeText;

    return freeText.isEmpty()
        ? attributesText
        : QString("%1 %2").arg(attributesText, freeText);
}

bool AnalyticsSearchListModel::isConstrained() const
{
    return filterRect().isValid()
        || !d->textFilter->text().isEmpty()
        || !selectedEngine().isNull()
        || !selectedObjectTypes().isEmpty()
        || !attributeFilters().empty()
        || base_type::isConstrained();
}

AnalyticsSearchListModel::LiveProcessingMode AnalyticsSearchListModel::liveProcessingMode() const
{
    return d->liveProcessingMode;
}

void AnalyticsSearchListModel::setLiveProcessingMode(LiveProcessingMode value)
{
    if (d->liveProcessingMode == value)
        return;

    // Live processing mode is not meant to be switched in the middle of work.
    // So just reset the model.
    clear();

    d->liveProcessingMode = value;
    NX_VERBOSE(this, "Set live processing mode to \"%1\"", value);

    setLiveSupported(d->liveProcessingMode == LiveProcessingMode::automaticAdd);
    emit availableNewTracksChanged();
}

bool AnalyticsSearchListModel::hasOnlyLiveCameras() const
{
    const QnVirtualCameraResourceSet& cameras = cameraSet().cameras();
    return !cameras.empty() && std::none_of(cameras.begin(), cameras.end(),
               [this](const QnVirtualCameraResourcePtr& camera) { return d->canViewArchive(camera); });
}

int AnalyticsSearchListModel::availableNewTracks() const
{
    if (d->liveProcessingMode == LiveProcessingMode::manualAdd)
        return 0;

    return d->newTrackCountUnknown
        ? kUnknownAvailableTrackCount
        : d->newTracks.size();
}

void AnalyticsSearchListModel::commitAvailableNewTracks()
{
    if (d->liveProcessingMode == LiveProcessingMode::manualAdd)
    {
        NX_VERBOSE(this, "Manual addition of available %1 new tracks was triggered",
            d->newTracks.size());
    }
    else
    {
        NX_VERBOSE(this, "Automatic addition of %1 live tracks", d->newTracks.size());
    }

    if (d->gapBeforeNewTracks)
    {
        NX_VERBOSE(this, "Clearing model due to the gap before new tracks");

        const QScopedValueRollback guard(d->keepLiveDataAtClear, true);
        clear();
        return;
    }

    if (d->newTracks.empty())
        return;

    NX_VERBOSE(this, "Live update commit");
    NX_ASSERT(detail::isSortedCorrectly<Facade>(d->newTracks.items,
        sortOrderFromDirection(FetchDirection::newer)));
    const auto centralPoint = d->data.items.empty()
        ?  Facade::startTime(d->data.items.front())
        : Facade::startTime(d->newTracks.items.front());
    d->applyFetchedData(std::move(d->newTracks.items),
        FetchRequest{.direction = FetchDirection::newer, .centralPointUs = centralPoint});
    d->newTracks.clear();

    if (d->liveProcessingMode == LiveProcessingMode::manualAdd)
        emit availableNewTracksChanged();
}

int AnalyticsSearchListModel::rowCount(const QModelIndex &parent) const
{
    return NX_ASSERT(!parent.isValid())
        ? d->data.size()
        : 0;
}

const ObjectTrack& AnalyticsSearchListModel::trackByRow(int row) const
{
    return d->data.items[row];
}

QnVirtualCameraResourcePtr AnalyticsSearchListModel::camera(const ObjectTrack& track) const
{
    const auto& deviceId = track.deviceId;
    const auto pool = resourcePool();
    if (NX_ASSERT(!deviceId.isNull()))
        return pool->getResourceById<QnVirtualCameraResource>(deviceId);

    // Fallback mechanism, just in case.

    return pool->getResourceById<QnVirtualCameraResource>(track.deviceId);
}

AnalyticsSearchListModel::PreviewParams AnalyticsSearchListModel::previewParams(
    const nx::analytics::db::ObjectTrack& track) const
{
    if (track.bestShot.initialized())
        return {microseconds(track.bestShot.timestampUs), track.bestShot.rect};

    const auto rect = track.bestShot.rect;
    return {microseconds(track.firstAppearanceTimeUs), rect};
}

QVariant AnalyticsSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(qBetween<int>(0, index.row(), d->data.size())))
        return {};

    const auto& track = trackByRow(index.row());

    switch (role)
    {
        case Qt::DisplayRole:
        {
            const auto fallbackTitle =
                [typeId = track.objectTypeId]()
                {
                    return QString("<%1>").arg(typeId.isEmpty() ? tr("Unknown track") : typeId);
                };

            const auto objectType = d->objectTypeById(track.objectTypeId);
            return objectType ? objectType->name() : fallbackTitle();
        }

        case HasExternalBestShotRole:
            return d->externalBestShotTracks.contains(track.id);

        case DecorationPathRole:
            return d->iconPath(track.objectTypeId);

        case Qt::DecorationRole:
        {
            const auto path = index.data(DecorationPathRole).toString();
            return path.isEmpty() ? QPixmap() : qnSkin->pixmap(path);
        }

        case DescriptionTextRole:
            return d->description(track);

        case AnalyticsAttributesRole:
            return QVariant::fromValue(systemContext()->analyticsAttributeHelper()->
                preprocessAttributes(track.objectTypeId, track.attributes));

        case TimestampRole:
            return QVariant::fromValue(microseconds(track.firstAppearanceTimeUs));

        case PreviewTimeRole:
            return QVariant::fromValue(previewParams(track).timestamp);

        case TimestampMsRole:
            return QVariant::fromValue(duration_cast<milliseconds>(
                microseconds(track.firstAppearanceTimeUs)).count());

        case PreviewStreamSelectionRole:
        {
            using namespace nx::vms::api;
            using StreamSelectionMode = ThumbnailFilter::StreamSelectionMode;

            const bool isBestShotCorrect = track.bestShot.initialized()
                && (track.bestShot.streamIndex == StreamIndex::primary
                    || track.bestShot.streamIndex == StreamIndex::secondary);

            if (!isBestShotCorrect)
                return QVariant::fromValue(StreamSelectionMode::forcedPrimary);

            return QVariant::fromValue(track.bestShot.streamIndex == StreamIndex::primary
                ? StreamSelectionMode::forcedPrimary
                : StreamSelectionMode::forcedSecondary);
        }
        case ObjectTrackIdRole:
            return QVariant::fromValue(track.id);

        case DurationRole:
            return QVariant::fromValue(objectDuration(track));

        case DurationMsRole:
            return QVariant::fromValue(duration_cast<milliseconds>(objectDuration(track)).count());

        case ResourceListRole:
        case DisplayedResourceListRole:
        {
            if (const auto resource = camera(track))
                return QVariant::fromValue(QnResourceList({resource}));

            if (role == DisplayedResourceListRole)
                return QVariant::fromValue(QStringList({QString("<%1>").arg(tr("deleted camera"))}));

            return {};
        }

        case ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(camera(track));

        case RawResourceRole:
        {
            const auto resource = camera(track);
            if (!resource)
                return {};

            QQmlEngine::setObjectOwnership(resource.get(), QQmlEngine::CppOwnership);
            return QVariant::fromValue(resource.get());
        }

        case AnalyticsEngineNameRole:
            return d->engineName(track);

        default:
            return base_type::data(index, role);
    }
}

bool AnalyticsSearchListModel::requestFetch(
    const FetchRequest& request,
    const FetchCompletionHandler& completionHandler)
{
    if (!(d->data.empty()
            || (request.direction == FetchDirection::older
                && request.centralPointUs == Facade::startTime(d->data.items.back()))
            || (request.direction == FetchDirection::newer
                && request.centralPointUs == Facade::startTime(d->data.items.front()))))
    {
        return false;
    }

    // We suppose we have the actual data for the moment of requst fetch call. So, as we can't have
    // analitics data removed and the data is actual we can just add new data to the curent one.
    const auto dataReceived =
        [this, request, completionHandler, currentItems = d->data.items]
            (bool success, rest::Handle requestId, LookupResult&& data)
        {
            NX_VERBOSE(this, "Received reply on request %1, result has %2 elements",
                requestId, data.size());

            if (!requestId || requestId != d->currentHttpRequestId)
                return;

            d->currentHttpRequestId = {};

            if (!success)
            {
                d->applyFetchedData({}, request);
                completionHandler(FetchResult::failed, {}, {});
                return;
            }

            completionHandler(FetchResult::complete,
                d->applyFetchedData(std::move(data), request),
                timeWindow<Facade>(d->data.items));
        };

    d->currentHttpRequestId = d->getObjects(request, maximumCount() / 2, dataReceived);
    return true;
}

void AnalyticsSearchListModel::clearData()
{
    const auto availableNewTracksGuard = d->makeAvailableNewTracksGuard();

    ScopedReset reset(this, !d->data.empty());
    if (d->keepLiveDataAtClear)
    {
        subtractKeysFromSet(/*minuend*/ d->externalBestShotTracks,
            /*subtrahend*/ d->data.idToTimestamp);
    }
    else
    {
        d->newTracks.clear();
        d->noBestShotTracks.clear();
        d->externalBestShotTracks.clear();
    }

    d->currentHttpRequestId = {};
    d->data.clear();
    d->gapBeforeNewTracks = false;
    d->newTrackCountUnknown = false;
}

void AnalyticsSearchListModel::setLiveTimestampGetter(LiveTimestampGetter value)
{
    d->liveTimestampGetter = value;
}

bool AnalyticsSearchListModel::hasAccessRights() const
{
    return systemContext()->accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewArchive);
}

bool AnalyticsSearchListModel::isFilterDegenerate() const
{
    return AbstractAsyncSearchListModel::isFilterDegenerate() || hasOnlyLiveCameras();
}

} // namespace nx::vms::client::core
