// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_list_model_p.h"

#include <algorithm>
#include <chrono>
#include <vector>

#include <QtCore/QScopedValueRollback>
// QMenu is the only widget allowed in Right Panel item models.
// It might be refactored later to avoid using QtWidgets at all.
#include <QtWidgets/QMenu>

#include <analytics/common/object_metadata.h>
#include <analytics/db/analytics_db_types.h>
#include <api/server_rest_connection.h>
#include <client/client_module.h>
#include <common/common_module.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/action_type_descriptor_manager.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/helpers.h>
#include <nx/analytics/taxonomy/object_type_dictionary.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/utils/datetime.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/format.h>
#include <nx/utils/log/log.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_icon_manager.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/analytics/taxonomy/utils.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/vms/text/human_readable.h>
#include <server/server_storage_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using namespace nx::analytics::db;
using namespace nx::vms::api::analytics;

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

using StreamIndex = nx::vms::api::StreamIndex;
using StreamSelectionMode = nx::api::CameraImageRequest::StreamSelectionMode;

static constexpr milliseconds kMetadataTimerInterval = 1000ms;
static constexpr milliseconds kDataChangedInterval = 500ms;

milliseconds startTime(const ObjectTrack& track)
{
    return duration_cast<milliseconds>(microseconds(track.firstAppearanceTimeUs));
}

microseconds objectDuration(const ObjectTrack& track)
{
    // TODO: #vkutin Is this duration formula good enough for us?
    //   Or we need to add some "lastAppearanceDurationUsec"?
    return microseconds(track.lastAppearanceTimeUs - track.firstAppearanceTimeUs);
}

template<class Set, class Hash>
void subtractKeysFromSet(Set& minuend, const Hash& subtrahend)
{
    for (const auto& [key, value]: nx::utils::constKeyValueRange(subtrahend))
        minuend.remove(key);
}

} // namespace

int AnalyticsSearchListModel::Private::Storage::indexOf(const QnUuid& trackId) const
{
    const auto timestampIter = idToTimestamp.find(trackId);
    if (timestampIter == idToTimestamp.end())
        return -1;

    const auto iter = std::lower_bound(
        items.cbegin(), items.cend(),
        std::make_pair(*timestampIter, trackId),
        [](const ObjectTrack& left, std::pair<milliseconds, QnUuid> right)
        {
            const auto leftTime = startTime(left);
            return leftTime > right.first || (leftTime == right.first && left.id > right.second);
        });

    return iter != items.cend() && iter->id == trackId
        ? (int) std::distance(items.cbegin(), iter)
        : -1;
}

int AnalyticsSearchListModel::Private::Storage::insert(
    nx::analytics::db::ObjectTrack&& item,
    std::function<void(int index)> notifyBeforeInsertion,
    std::function<void()> notifyAfterInsertion)
{
    const auto position = std::upper_bound(items.begin(), items.end(), item,
        [](const ObjectTrack& left, const ObjectTrack& right)
        {
            const auto leftTime = startTime(left);
            const auto rightTime = startTime(right);
            return leftTime > rightTime || (leftTime == rightTime && left.id > right.id);
        });

    const int index = position - items.begin();

    if (notifyBeforeInsertion)
        notifyBeforeInsertion(index);

    idToTimestamp[item.id] = startTime(item);
    items.insert(position, std::move(item));

    if (notifyAfterInsertion)
        notifyAfterInsertion();

    return index;
}

nx::analytics::db::ObjectTrack AnalyticsSearchListModel::Private::Storage::take(int index)
{
    nx::analytics::db::ObjectTrack result(std::move(items[index]));
    items.erase(items.begin() + index);
    idToTimestamp.remove(result.id);
    return result;
}

void AnalyticsSearchListModel::Private::Storage::clear()
{
    items.clear();
    idToTimestamp.clear();
}

AnalyticsSearchListModel::Private::Private(AnalyticsSearchListModel* q):
    base_type(q),
    q(q),
    m_emitDataChanged(new nx::utils::PendingOperation([this] { emitDataChangedIfNeeded(); },
        kDataChangedInterval.count(), this)),
    m_metadataProcessingTimer(new QTimer())
{
    m_emitDataChanged->setFlags(nx::utils::PendingOperation::NoFlags);

    m_metadataProcessingTimer->setInterval(kMetadataTimerInterval.count());
    connect(m_metadataProcessingTimer.data(), &QTimer::timeout, this, &Private::processMetadata);
    m_metadataProcessingTimer->start();

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
        [this](FetchResult result, FetchDirection direction)
        {
            if (result == FetchResult::complete && direction == FetchDirection::later)
                m_gapBeforeNewTracks = false;
        });

    connect(q->accessController(), &AccessController::permissionsMaybeChanged, this,
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
        new core::SessionResourcesSignalListener<QnVirtualCameraResource>(q);

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

    connect(qnServerStorageManager, &QnServerStorageManager::activeMetadataStorageChanged, this,
        [this](const QnMediaServerResourcePtr& server)
        {
            const auto relevantCameras = this->q->cameras();
            const auto serverFootageCameras =
                this->q->cameraHistoryPool()->getServerFootageCameras(server);

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
                m_objectTypeAcceptanceCache.clear();
                updateRelevantObjectTypes();
            });
    }
}

AnalyticsSearchListModel::Private::~Private()
{
}

int AnalyticsSearchListModel::Private::count() const
{
    return m_data.size();
}

QVariant AnalyticsSearchListModel::Private::data(const QModelIndex& index, int role,
    bool& handled) const
{
    const nx::analytics::db::ObjectTrack& track = m_data.items[index.row()];
    handled = true;

    switch (role)
    {
        case Qt::DisplayRole:
        {
            const auto fallbackTitle =
                [typeId = track.objectTypeId]()
                {
                    return QString("<%1>").arg(typeId.isEmpty() ? tr("Unknown track") : typeId);
                };

            const auto objectType = objectTypeById(track.objectTypeId);
            return objectType ? objectType->name() : fallbackTitle();
        }

        case Qn::HasExternalBestShotRole:
            return m_externalBestShotTracks.contains(track.id);

        case Qn::DecorationPathRole:
            return iconPath(track.objectTypeId);

        case Qn::DescriptionTextRole:
            return description(track);

        case Qn::AnalyticsAttributesRole:
            return QVariant::fromValue(qnClientModule->analyticsAttributeHelper()->
                preprocessAttributes(track.objectTypeId, track.attributes));

        case Qn::TimestampRole:
            return QVariant::fromValue(std::chrono::microseconds(track.firstAppearanceTimeUs));

        case Qn::PreviewTimeRole:
            return QVariant::fromValue(previewParams(track).timestamp);

        case Qn::PreviewStreamSelectionRole:
        {
            const bool isBestShotCorrect = track.bestShot.initialized()
                && (track.bestShot.streamIndex == StreamIndex::primary
                    || track.bestShot.streamIndex == StreamIndex::secondary);

            if (!isBestShotCorrect)
                return QVariant::fromValue(StreamSelectionMode::forcedPrimary);

            return QVariant::fromValue(track.bestShot.streamIndex == StreamIndex::primary
                ? StreamSelectionMode::forcedPrimary
                : StreamSelectionMode::forcedSecondary);
        }
        case Qn::ObjectTrackIdRole:
            return QVariant::fromValue(track.id);

        case Qn::DurationRole:
            return QVariant::fromValue(objectDuration(track));

        case Qn::HelpTopicIdRole:
            return HelpTopic::Id::Empty;

        case Qn::ResourceListRole:
        case Qn::DisplayedResourceListRole:
        {
            if (const auto resource = camera(track))
                return QVariant::fromValue(QnResourceList({resource}));

            if (role == Qn::DisplayedResourceListRole)
                return QVariant::fromValue(QStringList({QString("<%1>").arg(tr("deleted camera"))}));

            return {};
        }

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(camera(track));

        case Qn::ItemZoomRectRole:
            return QVariant::fromValue(previewParams(track).boundingBox);

        case Qn::AnalyticsEngineNameRole:
            return engineName(track);

        case Qn::CreateContextMenuRole:
            return QVariant::fromValue(contextMenu(track));

        default:
            handled = false;
            return {};
    }
}

QRectF AnalyticsSearchListModel::Private::filterRect() const
{
    return m_filterRect;
}

void AnalyticsSearchListModel::Private::setFilterRect(const QRectF& relativeRect)
{
    if (m_filterRect == relativeRect)
        return;

    q->clear();
    m_filterRect = relativeRect;
    NX_VERBOSE(q, "Set filter rect to \"%1\"", m_filterRect);
    emit q->filterRectChanged();
}

QStringList AnalyticsSearchListModel::Private::selectedObjectTypes() const
{
    return m_selectedObjectTypes;
}

void AnalyticsSearchListModel::Private::setSelectedObjectTypes(const QStringList& value)
{
    if (m_selectedObjectTypes == value)
        return;

    q->clear();
    m_selectedObjectTypes = value;
    NX_VERBOSE(q, "Set selected object type to \"%1\"", m_selectedObjectTypes);
    emit q->selectedObjectTypesChanged();

    updateRelevantObjectTypes();
}

QnUuid AnalyticsSearchListModel::Private::selectedEngine() const
{
    return m_selectedEngine;
}

void AnalyticsSearchListModel::Private::setSelectedEngine(const QnUuid& value)
{
    if (m_selectedEngine == value)
        return;

    q->clear();
    m_selectedEngine = value;
    NX_VERBOSE(q, "Set selected engine to %1", m_selectedEngine.toString());
    emit q->selectedEngineChanged();
}

const std::set<QString>& AnalyticsSearchListModel::Private::relevantObjectTypes() const
{
    return m_relevantObjectTypes;
}

QStringList AnalyticsSearchListModel::Private::attributeFilters() const
{
    return m_attributeFilters;
}

void AnalyticsSearchListModel::Private::setAttributeFilters(const QStringList& value)
{
    if (m_attributeFilters == value)
        return;

    q->clear();
    m_attributeFilters = value;
    NX_VERBOSE(q, "Set attribute filters to \"%1\"", m_attributeFilters.join(" "));
    emit q->attributeFiltersChanged();
}

void AnalyticsSearchListModel::Private::updateRelevantObjectTypes()
{
    const auto relevantObjectTypes = nx::analytics::db::addDerivedTypeIds(
        q->systemContext()->analyticsTaxonomyStateWatcher(), m_selectedObjectTypes);

    if (relevantObjectTypes == m_relevantObjectTypes)
        return;

    m_relevantObjectTypes = relevantObjectTypes;
    emit q->relevantObjectTypesChanged();
}

nx::utils::Guard AnalyticsSearchListModel::Private::makeAvailableNewTracksGuard()
{
    return nx::utils::Guard(
        [this, oldValue = availableNewTracks()]()
        {
            if (oldValue != availableNewTracks())
                emit q->availableNewTracksChanged();
        });
}

void AnalyticsSearchListModel::Private::clearData()
{
    const auto availableNewTracksGuard = makeAvailableNewTracksGuard();

    ScopedReset reset(q, !m_data.empty());
    if (m_keepLiveDataAtClear)
    {
        subtractKeysFromSet(/*minuend*/ m_externalBestShotTracks,
            /*subtrahend*/ m_data.idToTimestamp);
    }
    else
    {
        m_newTracks.clear();
        m_noBestShotTracks.clear();
        m_externalBestShotTracks.clear();
    }

    m_data.clear();
    m_prefetch.clear();
    m_gapBeforeNewTracks = false;
    m_newTrackCountUnknown = false;
}

void AnalyticsSearchListModel::Private::truncateToMaximumCount()
{
    if (!q->truncateDataToMaximumCount(m_data.items, &startTime, itemCleanupFunction(m_data)))
        return;

    if (q->fetchDirection() == FetchDirection::earlier)
    {
        m_gapBeforeNewTracks = true;

        if (m_liveProcessingMode == LiveProcessingMode::automaticAdd)
            q->setLive(false);
    }
}

void AnalyticsSearchListModel::Private::truncateToRelevantTimePeriod()
{
    q->truncateDataToTimePeriod(
        m_data.items, &startTime, q->relevantTimePeriod(), itemCleanupFunction(m_data));
}

bool AnalyticsSearchListModel::Private::canViewArchive(
    const QnVirtualCameraResourcePtr& camera) const
{
    return q->accessController()->hasPermissions(camera, Qn::ViewFootagePermission);
}

QString AnalyticsSearchListModel::Private::makeEnumValuesExact(const QString& filter)
{
    return analytics::taxonomy::makeEnumValuesExact(
        filter,
        qnClientModule->analyticsAttributeHelper(),
        {m_relevantObjectTypes.begin(), m_relevantObjectTypes.end()});
}

AnalyticsSearchListModel::ItemCleanupFunction<nx::analytics::db::ObjectTrack>
    AnalyticsSearchListModel::Private::itemCleanupFunction(Storage& storage)
{
    const auto cleanupFunction =
        [this, &storage](const nx::analytics::db::ObjectTrack& track)
        {
            storage.idToTimestamp.remove(track.id);
            m_externalBestShotTracks.remove(track.id);
        };

    return cleanupFunction;
}

rest::Handle AnalyticsSearchListModel::Private::requestPrefetch(const QnTimePeriod& period)
{
    const auto dataReceived =
        [this](bool success, rest::Handle requestId, LookupResult&& data)
        {
            NX_VERBOSE(this, "Received reply on request %1, result has %2 elements",
                requestId, data.size());

            if (!requestId || requestId != currentRequest().id)
                return;

            QnTimePeriod actuallyFetched;
            m_prefetch = LookupResult();

            if (success)
            {
                m_prefetch = std::move(data);

                NX_VERBOSE(this, "Processing %1 loaded tracks", m_prefetch.size());
                if (!m_prefetch.empty())
                {
                    actuallyFetched = QnTimePeriod::fromInterval(
                        startTime(m_prefetch.back()), startTime(m_prefetch.front()));
                }
            }

            completePrefetch(actuallyFetched, success, (int)m_prefetch.size());
        };

    return getObjects(period, dataReceived, currentRequest().batchSize);
}

template<typename Iter>
bool AnalyticsSearchListModel::Private::commitInternal(const QnTimePeriod& periodToCommit,
    Iter prefetchBegin, Iter prefetchEnd, int position, bool handleOverlaps)
{
    const auto begin = std::lower_bound(prefetchBegin, prefetchEnd, periodToCommit.endTime(),
        [](const ObjectTrack& left, milliseconds right)
        {
            return startTime(left) > right;
        });

    auto end = std::upper_bound(prefetchBegin, prefetchEnd, periodToCommit.startTime(),
        [](milliseconds left, const ObjectTrack& right)
        {
            return left > startTime(right);
        });

    int updated = 0;
    int insertedToMiddle = 0;

    if (handleOverlaps && !m_data.empty())
    {
        const auto lastItem = m_data.items.front();
        const auto lastTime = startTime(lastItem);

        while (end != begin)
        {
            const auto iter = end - 1;
            const auto time = startTime(*iter);

            if (time > lastTime || (time == lastTime && iter->id > lastItem.id))
                break;

            const int index = m_data.indexOf(iter->id);
            if (index >= 0)
            {
                ++updated;
                m_dataChangedTrackIds.insert(iter->id);
                m_emitDataChanged->requestOperation();
            }
            else
            {
                ++insertedToMiddle;
                m_data.insert(std::move(*iter),
                    [this](int posToInsert) { q->beginInsertRows({}, posToInsert, posToInsert); },
                    [this]() { q->endInsertRows(); });
            }

            end = iter;
        }
    }

    if (updated > 0)
        NX_VERBOSE(q, "Updated %1 tracks", updated);

    if (insertedToMiddle > 0)
        NX_VERBOSE(q, "%1 tracks inserted in the middle", insertedToMiddle);

    const auto count = std::distance(begin, end);
    if (count <= 0)
    {
        NX_VERBOSE(q, "Committing no tracks");
        return false;
    }

    NX_VERBOSE(q, "Committing %1 tracks:\n    from: %2\n    to: %3", count,
        nx::utils::timestampToDebugString(startTime(*(end - 1)).count()),
        nx::utils::timestampToDebugString(startTime(*begin).count()));

    ScopedInsertRows insertRows(q, position, position + count - 1);
    for (auto iter = begin; iter != end; ++iter)
    {
        if (!m_data.idToTimestamp.contains(iter->id)) //< Just to be safe.
            m_data.idToTimestamp[iter->id] = startTime(*iter);
    }

    m_data.items.insert(m_data.items.begin() + position,
        std::make_move_iterator(begin), std::make_move_iterator(end));

    return true;
}

bool AnalyticsSearchListModel::Private::commitPrefetch(const QnTimePeriod& periodToCommit)
{
    const auto clearPrefetch = nx::utils::makeScopeGuard([this]() { m_prefetch.clear(); });

    if (currentRequest().direction == FetchDirection::earlier)
        return commitInternal(periodToCommit, m_prefetch.begin(), m_prefetch.end(), count(), false);

    NX_ASSERT(currentRequest().direction == FetchDirection::later);
    return commitInternal(
        periodToCommit, m_prefetch.rbegin(), m_prefetch.rend(), 0, q->effectiveLiveSupported());
}

rest::Handle AnalyticsSearchListModel::Private::getObjects(const QnTimePeriod& period,
    GetCallback callback, int limit) const
{
    if (!NX_ASSERT(callback && connection() && !q->isFilterDegenerate()))
        return {};

    Filter request;
    if (q->cameraSet()->type() != ManagedCameraSet::Type::all)
    {
        for (const auto& camera: q->cameraSet()->cameras())
        {
            if (canViewArchive(camera))
                request.deviceIds.insert(camera->getId());
        }

        if (!NX_ASSERT(!request.deviceIds.empty()))
            return {};
    }

    request.timePeriod = period;
    request.maxObjectTracksToSelect = limit;
    request.freeText = q->combinedTextFilter();
    request.analyticsEngineId = m_selectedEngine;

    if (!m_selectedObjectTypes.isEmpty())
    {
        request.objectTypeId =
            std::set(m_selectedObjectTypes.begin(), m_selectedObjectTypes.end());
    }

    if (q->cameraSet()->type() == ManagedCameraSet::Type::single && m_filterRect.isValid())
        request.boundingBox = m_filterRect;

    request.sortOrder = currentRequest().direction == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    NX_VERBOSE(q, "Requesting object tracks:\n    from: %1\n    to: %2\n"
        "    box: %3\n    text filter: %4\n    sort: %5\n    limit: %6",
        nx::utils::timestampToDebugString(period.startTimeMs),
        nx::utils::timestampToDebugString(period.endTimeMs()),
        request.boundingBox,
        request.freeText,
        QVariant::fromValue(request.sortOrder).toString(),
        request.maxObjectTracksToSelect);

    return lookupObjectTracksCached(request, nx::utils::guarded(this, callback));
}

rest::Handle AnalyticsSearchListModel::Private::lookupObjectTracksCached(
    const nx::analytics::db::Filter& request,
    GetCallback callback) const
{
    if (!NX_ASSERT(thread() == QThread::currentThread()))
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
        const auto responseHandler = nx::utils::guarded(this,
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

        const auto api = connectedServerApi();
        if (!api)
            return {};

        context.handle = api->lookupObjectTracks(
            request, /*isLocal*/ false, responseHandler, thread());

        NX_VERBOSE(q, "Created a new request, handle=%1", context.handle);
        cachedRequests.push_back(context);
        return context.handle;
    }
}

void AnalyticsSearchListModel::Private::setLiveTimestampGetter(LiveTimestampGetter value)
{
    m_liveTimestampGetter = value;
}

void AnalyticsSearchListModel::Private::updateMetadataReceivers()
{
    if (m_liveReceptionActive)
    {
        auto cameras = q->cameras();
        MetadataReceiverList newMetadataReceivers;

        // Remove no longer relevant deferred metadata.
        const auto deferredCameras = m_deferredMetadata.keys();
        for (const auto& camera: deferredCameras)
        {
            if (!cameras.contains(camera))
                m_deferredMetadata.remove(camera);
        }

        const auto isReceiverRequired =
            [this](const QnVirtualCameraResourcePtr& camera)
            {
                return camera->isOnline()
                    && !camera->enabledAnalyticsEngines().empty()
                    && q->accessController()->hasPermissions(camera, Qn::ViewFootagePermission);
            };

        // Preserve existing receivers that are still relevant.
        for (auto& receiver: m_metadataReceivers)
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
        m_metadataReceivers = std::move(newMetadataReceivers);
    }
    else
    {
        m_metadataReceivers.clear();
        m_deferredMetadata.clear();
        NX_VERBOSE(q, "Released all metadata receivers");
    }
}

void AnalyticsSearchListModel::Private::setLiveReceptionActive(bool value)
{
    if (m_liveReceptionActive == value)
        return;

    NX_VERBOSE(q, "Setting live reception %1", (value ? "active" : "inactive"));
    m_liveReceptionActive = value;

    updateMetadataReceivers();
}


AnalyticsSearchListModel::LiveProcessingMode
    AnalyticsSearchListModel::Private::liveProcessingMode() const
{
    return m_liveProcessingMode;
}

void AnalyticsSearchListModel::Private::setLiveProcessingMode(LiveProcessingMode value)
{
    if (m_liveProcessingMode == value)
        return;

    // Live processing mode is not meant to be switched in the middle of work.
    // So just reset the model.
    q->clear();

    m_liveProcessingMode = value;
    NX_VERBOSE(q, "Set live processing mode to \"%1\"", value);

    q->setLiveSupported(m_liveProcessingMode == LiveProcessingMode::automaticAdd);
    emit q->availableNewTracksChanged();
}

int AnalyticsSearchListModel::Private::availableNewTracks() const
{
    return m_liveProcessingMode == LiveProcessingMode::manualAdd
        ? (m_newTrackCountUnknown ? kUnknownAvailableTrackCount : m_newTracks.size())
        : 0;
}

bool AnalyticsSearchListModel::Private::isAcceptedObjectType(const QString& typeId) const
{
    if (!m_objectTypeAcceptanceCache.contains(typeId))
    {
        const auto objectType = objectTypeById(typeId);
        m_objectTypeAcceptanceCache[typeId] =
            objectType && !objectType->isLiveOnly() && !objectType->isNonIndexable();
    }

    return m_objectTypeAcceptanceCache.value(typeId);
}

void AnalyticsSearchListModel::Private::processMetadata()
{
    // Don't start receiving live data until first archive fetch is finished.
    if (m_data.empty() && (fetchInProgress() || q->canFetchData()))
        return;

    // Special handling if live mode is paused (analytics tab is hidden).
    if (q->livePaused() && !m_data.empty())
    {
        m_gapBeforeNewTracks = true;

        if (m_liveProcessingMode == LiveProcessingMode::automaticAdd)
        {
            // Completely stop metadata processing in automatic mode.
            q->setLive(false);
        }
        else
        {
            const auto availableNewTracksGuard = makeAvailableNewTracksGuard();
            m_newTrackCountUnknown = m_newTrackCountUnknown || !m_newTracks.empty();

            // Discard previously received tracks in manual mode.
            subtractKeysFromSet(/*minuend*/ m_externalBestShotTracks,
                /*subtrahend*/ m_newTracks.idToTimestamp);
            subtractKeysFromSet(/*minuend*/ m_externalBestShotTracks,
                /*subtrahend*/ m_noBestShotTracks.idToTimestamp);
            m_newTracks.clear();
            m_noBestShotTracks.clear();
        }
    }

    setLiveReceptionActive(
        (q->isLive() || m_liveProcessingMode == LiveProcessingMode::manualAdd)
        && q->relevantTimePeriod().isInfinite() //< This check is required for manualAdd mode.
        && q->isOnline()
        && !q->livePaused()
        && (!q->isFilterDegenerate() || q->hasOnlyLiveCameras()));

    if (!m_liveReceptionActive)
        return;

    // Fetch all metadata packets from receiver buffers.

    std::vector<QnAbstractCompressedMetadataPtr> metadataPackets;
    int numSources = 0;

    static constexpr auto kMaxTimestamp = std::numeric_limits<milliseconds>::max();

    milliseconds liveTimestamp{};
    if (m_liveTimestampGetter && ini().delayRightPanelLiveAnalytics)
    {
        // Choose "live" of a camera with the longest delay.
        for (const auto& receiver: m_metadataReceivers)
        {
            const auto timestamp = m_liveTimestampGetter(receiver->camera());
            if (timestamp > 0ms && timestamp != kMaxTimestamp)
                liveTimestamp = std::max(liveTimestamp, timestamp);
        }
    }

    decltype(m_deferredMetadata) currentMetadata;
    currentMetadata.swap(m_deferredMetadata);

    for (const auto& receiver: m_metadataReceivers)
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
                m_deferredMetadata[camera].swap(packets);
            }
            else if (pos != packets.end())
            {
                m_deferredMetadata[camera] = packets.mid(pos - packets.begin());
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
        Storage* storage = nullptr;
        int index = -1;
    };

    const auto findObject =
        [this](const QnUuid& trackId) -> FoundObjectTrack
        {
            int index = m_newTracks.indexOf(trackId);
            if (index >= 0)
                return {&m_newTracks, index};

            index = m_data.indexOf(trackId);
            if (index >= 0)
                return {&m_data, index};

            index = m_noBestShotTracks.indexOf(trackId);
            if (index >= 0)
                return {&m_noBestShotTracks, index};

            return {};
        };

    nx::analytics::db::Filter filter;
    filter.freeText = q->combinedTextFilter();
    filter.objectTypeId = m_relevantObjectTypes;
    filter.analyticsEngineId = m_selectedEngine;

    if (m_filterRect.isValid())
        filter.boundingBox = m_filterRect;

    const nx::analytics::taxonomy::ObjectTypeDictionary objectTypeDictionary(
        q->systemContext()->analyticsTaxonomyStateWatcher());

    const int oldNewTrackCount = m_newTracks.size();

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
                    m_externalBestShotTracks.insert(item.trackId);
                }

                auto& track = found.storage->items[found.index];
                track.bestShot.timestampUs = objectMetadata->timestampUs;
                track.bestShot.rect = item.boundingBox;
                track.bestShot.streamIndex  = objectMetadata->streamIndex;

                if (found.storage == &m_data)
                {
                    // If it was an already loaded track, emit dataChanged.
                    m_dataChangedTrackIds.insert(track.id);
                    m_emitDataChanged->requestOperation();
                }
                else if (found.storage == &m_noBestShotTracks)
                {
                    // If it was a no-best-shot track, move it to the newly added.
                    const int index = m_newTracks.insert(m_noBestShotTracks.take(found.index));
                    found = FoundObjectTrack{&m_newTracks, index};
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
                advanceTrack(track, std::move(pos), /*emitDataChanged*/ found.storage == &m_data);
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
                m_newTracks.insert(std::move(newTrack));
            else
                m_noBestShotTracks.insert(std::move(newTrack));
        }
    }

    // Limit the size of the no-best-shot track queue.
    if (m_noBestShotTracks.size() > q->maximumCount())
    {
        const auto removeBegin = m_noBestShotTracks.items.begin() + q->maximumCount();
        const auto cleanupFunction = itemCleanupFunction(m_noBestShotTracks);
        std::for_each(removeBegin, m_noBestShotTracks.items.end(), cleanupFunction);
        m_noBestShotTracks.items.erase(removeBegin, m_noBestShotTracks.items.end());
    }

    // Limit the size of the available new tracks queue.
    if (m_newTracks.size() > q->maximumCount())
    {
        const auto removeBegin = m_newTracks.items.begin() + q->maximumCount();
        const auto cleanupFunction = itemCleanupFunction(m_newTracks);
        std::for_each(removeBegin, m_newTracks.items.end(), cleanupFunction);
        m_newTracks.items.erase(removeBegin, m_newTracks.items.end());
        m_gapBeforeNewTracks = true;
    }

    if (m_newTracks.size() == oldNewTrackCount)
        return;

    NX_VERBOSE(q, "Detected %1 new object tracks", m_newTracks.size() - oldNewTrackCount);

    if (m_liveProcessingMode == LiveProcessingMode::manualAdd)
        emit q->availableNewTracksChanged();
    else
        commitNewTracks();
}

void AnalyticsSearchListModel::Private::commitNewTracks()
{
    if (m_liveProcessingMode == LiveProcessingMode::manualAdd)
        NX_VERBOSE(q, "Manual addition of available %1 new tracks was triggered", m_newTracks.size());
    else
        NX_VERBOSE(q, "Automatic addition of %1 live tracks", m_newTracks.size());

    if (m_gapBeforeNewTracks)
    {
        NX_VERBOSE(q, "Clearing model due to the gap before new tracks");

        QScopedValueRollback guard(m_keepLiveDataAtClear, true);
        q->clear();
        return;
    }

    if (m_newTracks.empty())
        return;

    ScopedLiveCommit liveCommit(q);

    const auto periodToCommit = QnTimePeriod::fromInterval(
        startTime(m_newTracks.items.front()), startTime(m_newTracks.items.back()));

    q->addToFetchedTimeWindow(periodToCommit);

    NX_VERBOSE(q, "Live update commit");
    commitInternal(periodToCommit,
        m_newTracks.items.begin(),
        m_newTracks.items.end(),
        /*position*/ 0,
        /*handleOverlaps*/ true);

    m_newTracks.clear();

    if (m_liveProcessingMode == LiveProcessingMode::manualAdd)
        emit q->availableNewTracksChanged();

    if (count() > q->maximumCount())
    {
        const auto directionGuard = nx::utils::makeScopeGuard(
            [this, direction = q->fetchDirection()]
            {
                q->setFetchDirection(direction);
            });

        if (m_liveProcessingMode == LiveProcessingMode::manualAdd)
            q->setFetchDirection(FetchDirection::later); //< To avoid truncation of added tracks.

        NX_VERBOSE(q, "Truncating to maximum count");
        truncateToMaximumCount();
    }
}

void AnalyticsSearchListModel::Private::emitDataChangedIfNeeded()
{
    if (m_dataChangedTrackIds.empty())
        return;

    for (const auto& id: m_dataChangedTrackIds)
    {
        const auto index = m_data.indexOf(id);
        if (index < 0)
            continue;

        const auto modelIndex = q->index(index);
        emit q->dataChanged(modelIndex, modelIndex);
    }

    m_dataChangedTrackIds.clear();
};

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
        m_dataChangedTrackIds.insert(track.id);
        m_emitDataChanged->requestOperation();
    }
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
    const auto iconManager = core::analytics::IconManager::instance();
    const auto objectType = objectTypeById(objectTypeId);

    return objectType
        ? iconManager->iconPath(objectType->icon())
        : iconManager->fallbackIconPath();
}

QString AnalyticsSearchListModel::Private::description(
    const ObjectTrack& track) const
{
    if (!ini().showDebugTimeInformationInRibbon)
        return QString();


    // TODO: #sivanov Actualize used system context.
    const auto timeWatcher = appContext()->currentSystemContext()->serverTimeWatcher();
    const auto start = timeWatcher->displayTime(startTime(track).count());
    const auto duration = objectDuration(track);

    using namespace std::chrono;
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
        return QString();

    if (const auto engine = taxonomyState->engineById(track.analyticsEngineId.toString()))
        return engine->name();

    return QString();
}

QSharedPointer<QMenu> AnalyticsSearchListModel::Private::contextMenu(
    const ObjectTrack& track) const
{
    const auto camera = this->camera(track);
    if (!camera)
        return {};

    const nx::analytics::ActionTypeDescriptorManager descriptorManager(q->systemContext());
    const auto actionByEngine = descriptorManager.availableObjectActionTypeDescriptors(
        track.objectTypeId,
        camera);

    QSharedPointer<QMenu> menu(new QMenu());
    for (const auto& [engineId, actionById]: actionByEngine)
    {
        if (!menu->isEmpty())
            menu->addSeparator();

        for (const auto& [actionId, actionDescriptor]: actionById)
        {
            const auto name = actionDescriptor.name;
            menu->addAction<std::function<void()>>(name, nx::utils::guarded(this,
                [this, engineId = engineId, actionId = actionDescriptor.id, track, camera]()
                {
                    emit q->pluginActionRequested(engineId, actionId, track, camera,
                        AnalyticsSearchListModel::QPrivateSignal());
                }));
        }
    }

    if (menu->isEmpty())
        menu.reset();

    return menu;
}

AnalyticsSearchListModel::Private::PreviewParams AnalyticsSearchListModel::Private::previewParams(
    const ObjectTrack& track)
{
    if (track.bestShot.initialized())
        return {microseconds(track.bestShot.timestampUs), track.bestShot.rect};

    const auto rect = track.bestShot.rect;
    return {microseconds(track.firstAppearanceTimeUs), rect};
}

QnVirtualCameraResourcePtr AnalyticsSearchListModel::Private::camera(
    const ObjectTrack& track) const
{
    const auto& deviceId = track.deviceId;
    if (NX_ASSERT(!deviceId.isNull()))
        return q->resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId);

    // Fallback mechanism, just in case.

    return q->resourcePool()->getResourceById<QnVirtualCameraResource>(
        track.deviceId);
}

} // namespace nx::vms::client::desktop
