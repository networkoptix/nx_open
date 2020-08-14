#include "analytics_search_list_model_p.h"

#include <algorithm>
#include <chrono>

// QMenu is the only widget allowed in Right Panel item models.
// It might be refactored later to avoid using QtWidgets at all.
#include <QtWidgets/QMenu>

#include <api/server_rest_connection.h>
#include <analytics/common/object_metadata.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_changes_listener.h>
#include <server/server_storage_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <nx/client/core/watchers/server_time_watcher.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include <nx/analytics/descriptor_manager.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/client/desktop/analytics/analytics_attributes.h>
#include <nx/vms/client/desktop/analytics/object_type_dictionary.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/utils/datetime.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/log/log.h>

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
static constexpr milliseconds kUpdateWorkbenchFilterDelay = 100ms;

static constexpr int kMaxumumNoBestShotTrackCount = 10000;

static constexpr int kMaxDisplayedAttributeValues = 2; //< Before "(+n values)".

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

static const auto lowerBoundPredicate =
    [](const ObjectTrack& left, milliseconds right)
    {
        return startTime(left) > right;
    };

static const auto upperBoundPredicate =
    [](milliseconds left, const ObjectTrack& right)
    {
        return left > startTime(right);
    };

} // namespace

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

    connect(q, &AbstractSearchListModel::camerasChanged, this, &Private::updateMetadataReceivers);

    auto cameraStatusListener = new QnResourceChangesListener(q);
    cameraStatusListener->connectToResources<QnVirtualCameraResource>(
        q->resourcePool(),
        &QnResource::statusChanged,
        [this](const QnResourcePtr& resource)
        {
            if (!this->q->isOnline())
                return;

            if (isCameraApplicable(resource.dynamicCast<QnVirtualCameraResource>()))
                updateMetadataReceivers();
        });

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
}

AnalyticsSearchListModel::Private::~Private()
{
}

int AnalyticsSearchListModel::Private::count() const
{
    return int(m_data.size());
}

QVariant AnalyticsSearchListModel::Private::data(const QModelIndex& index, int role,
    bool& handled) const
{
    const nx::analytics::db::ObjectTrack& track = m_data[index.row()];
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

            const auto objectCamera = camera(track);
            if (!objectCamera)
                return fallbackTitle();

            const auto objectTypeDescriptor = objectCamera->commonModule()
                ->analyticsObjectTypeDescriptorManager()->descriptor(track.objectTypeId);

            if (!objectTypeDescriptor)
                return fallbackTitle();

            return objectTypeDescriptor->name.isEmpty()
                ? fallbackTitle()
                : objectTypeDescriptor->name;
        }

        case Qn::HasExternalBestShotRole:
            return m_externalBestShotTracks.contains(track.id);

        case Qt::DecorationRole:
            return QVariant::fromValue(qnSkin->pixmap("analytics/analytics.svg"));

        case Qn::DescriptionTextRole:
            return description(track);

        case Qn::AttributeTableRole:
            return QVariant::fromValue(attributes(track));

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
            return Qn::Empty_Help;

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

        case Qn::ContextMenuRole:
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
}

QString AnalyticsSearchListModel::Private::filterText() const
{
    return m_filterText;
}

void AnalyticsSearchListModel::Private::setFilterText(const QString& value)
{
    // Check is user input parsed to the same filter request.
    if (m_filterText == value)
        return;

    q->clear();
    m_filterText = value;
}

QString AnalyticsSearchListModel::Private::selectedObjectType() const
{
    return m_selectedObjectType;
}

void AnalyticsSearchListModel::Private::setSelectedObjectType(const QString& value)
{
    if (m_selectedObjectType == value)
        return;

    q->clear();
    m_selectedObjectType = value;
}

void AnalyticsSearchListModel::Private::clearData()
{
    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_objectTrackIdToTimestamp.clear();
    m_noBestShotTracks.clear();
    m_externalBestShotTracks.clear();
}

void AnalyticsSearchListModel::Private::truncateToMaximumCount()
{
    q->truncateDataToMaximumCount(m_data, &startTime,
        [this](const ObjectTrack& track) { handleItemRemoved(track); });
}

void AnalyticsSearchListModel::Private::truncateToRelevantTimePeriod()
{
    q->truncateDataToTimePeriod(m_data, &startTime, q->relevantTimePeriod(),
        [this](const ObjectTrack& track) { handleItemRemoved(track); });
}

void AnalyticsSearchListModel::Private::handleItemRemoved(const ObjectTrack& track)
{
    m_objectTrackIdToTimestamp.remove(track.id);
    m_externalBestShotTracks.remove(track.id);
}

bool AnalyticsSearchListModel::Private::isCameraApplicable(
    const QnVirtualCameraResourcePtr& camera) const
{
    // TODO: #vkutin Implement it when it's possible!
    NX_ASSERT(camera);
    return true;
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
    const auto begin = std::lower_bound(prefetchBegin, prefetchEnd,
        periodToCommit.endTime(), lowerBoundPredicate);

    auto end = std::upper_bound(prefetchBegin, prefetchEnd,
        periodToCommit.startTime(), upperBoundPredicate);

    if (handleOverlaps && !m_data.empty())
    {
        const auto& last = m_data.front();
        const auto lastTimeUs = last.firstAppearanceTimeUs;

        while (end != begin)
        {
            const auto iter = end - 1;
            const auto timeUs = iter->firstAppearanceTimeUs;

            if (timeUs > lastTimeUs)
                break;

            end = iter;

            if (timeUs == lastTimeUs && iter->id == last.id)
                break;
        }
    }

    const auto count = std::distance(begin, end);
    if (count <= 0)
    {
        NX_VERBOSE(q, "Committing no analytics");
        return false;
    }

    NX_VERBOSE(q, "Committing %1 analytics:\n    from: %2\n    to: %3", count,
        nx::utils::timestampToDebugString(startTime(*(end - 1)).count()),
        nx::utils::timestampToDebugString(startTime(*begin).count()));

    ScopedInsertRows insertRows(q, position, position + count - 1);
    for (auto iter = begin; iter != end; ++iter)
    {
        if (!m_objectTrackIdToTimestamp.contains(iter->id)) //< Just to be safe.
            m_objectTrackIdToTimestamp[iter->id] = startTime(*iter);
    }

    m_data.insert(m_data.begin() + position,
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
    const auto server = q->commonModule()->currentServer();
    if (!NX_ASSERT(callback && server && server->restConnection() && !q->isFilterDegenerate()))
        return {};

    Filter request;
    if (q->cameraSet()->type() != ManagedCameraSet::Type::all)
    {
        const auto cameras = q->cameraSet()->cameras();
        request.deviceIds.reserve(cameras.size());
        std::transform(cameras.cbegin(), cameras.cend(), std::back_inserter(request.deviceIds),
            [](const QnVirtualCameraResourcePtr& camera) { return camera->getId(); });
    }

    request.timePeriod = period;
    request.maxObjectTracksToSelect = limit;
    request.freeText = m_filterText;
    request.withBestShotOnly = true;

    if (!m_selectedObjectType.isEmpty())
        request.objectTypeId = {m_selectedObjectType};

    if (q->cameraSet()->type() == ManagedCameraSet::Type::single && m_filterRect.isValid())
        request.boundingBox = m_filterRect;

    request.sortOrder = currentRequest().direction == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    NX_VERBOSE(q, "Requesting analytics:\n    from: %1\n    to: %2\n"
        "    box: %3\n    text filter: %4\n    sort: %5\n    limit: %6\n"
        "    withBestShotOnly: %7",
        nx::utils::timestampToDebugString(period.startTimeMs),
        nx::utils::timestampToDebugString(period.endTimeMs()),
        request.boundingBox,
        request.freeText,
        QVariant::fromValue(request.sortOrder).toString(),
        request.maxObjectTracksToSelect,
        request.withBestShotOnly);

    return server->restConnection()->lookupObjectTracks(
        request, false /*isLocal*/, nx::utils::guarded(this, callback), thread());
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

        // Preserve existing receivers that are still relevant.
        for (auto& receiver: m_metadataReceivers)
        {
            if (cameras.remove(receiver->camera()) && receiver->camera()->isOnline())
                newMetadataReceivers.emplace_back(receiver.release());
        }

        // Create new receivers if needed.
        for (const auto& camera: cameras)
        {
            if (camera->isOnline())
            {
                newMetadataReceivers.emplace_back(new LiveAnalyticsReceiver(camera));

                connect(newMetadataReceivers.back().get(), &LiveAnalyticsReceiver::dataOverflow,
                    this, &Private::processMetadata);
            }
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

void AnalyticsSearchListModel::Private::processMetadata()
{
    // Don't start receiving live data until first archive fetch is finished.
    if (m_data.empty() && (fetchInProgress() || q->canFetchMore()))
        return;

    // Completely stop metadata reception if paused.
    if (q->livePaused() && !m_data.empty())
        q->setLive(false);

    setLiveReceptionActive(q->isLive() && !q->livePaused()
        && q->isOnline()
        && !q->isFilterDegenerate());

    if (!m_liveReceptionActive)
        return;

    // Fetch all metadata packets from receiver buffers.

    QList<QList<QnAbstractCompressedMetadataPtr>> packetsBySource;
    int totalPackets = 0;

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

    for (const auto& receiver: m_metadataReceivers)
    {
        auto packets = m_deferredMetadata.take(receiver->camera()) + receiver->takeData();
        if (packets.empty())
            continue;

        const auto metadataTimestamp =
            [this](const QnAbstractCompressedMetadataPtr& packet) -> milliseconds
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
                m_deferredMetadata[receiver->camera()].swap(packets);
            }
            else if (pos != packets.end())
            {
                m_deferredMetadata[receiver->camera()] = packets.mid(pos - packets.begin());
                packets.erase(pos, packets.end());
            }
        }

        packetsBySource.push_back(packets);
        totalPackets += packets.size();
    }

    if (totalPackets == 0)
        return;

    // Process all metadata packets.

    NX_VERBOSE(q, "Processing %1 live metadata packets from %2 sources",
        totalPackets, packetsBySource.size());

    std::vector<ObjectTrack> newTracks;
    newTracks.reserve(totalPackets);

    QHash<QnUuid, int> newObjectIndices;

    enum class TrackType
    {
        none,
        loaded,
        beingAdded,
        noBestShot
    };

    struct FoundObjectTrack
    {
        ObjectTrack* track = nullptr;
        int index = -1;
        TrackType type;
    };

    const auto findObject =
        [&](const QnUuid& trackId) -> FoundObjectTrack
        {
            auto index = newObjectIndices.value(trackId, -1);
            if (index >= 0)
                return {&newTracks[index], index, TrackType::beingAdded};

            index = indexOf(trackId);
            if (index >= 0)
                return {&m_data[index], index, TrackType::loaded};

            const auto it = std::find_if(m_noBestShotTracks.begin(), m_noBestShotTracks.end(),
                [trackId](const ObjectTrack& track) { return track.id == trackId; });

            if (it != m_noBestShotTracks.end())
                return {&*it, (int) (it - m_noBestShotTracks.begin()), TrackType::noBestShot};

            return {};
        };

    nx::analytics::db::Filter filter;
    filter.freeText = m_filterText;
    filter.withBestShotOnly = false;
    if (m_filterRect.isValid())
        filter.boundingBox = m_filterRect;
    if (!m_selectedObjectType.isEmpty())
        filter.objectTypeId.push_back(m_selectedObjectType);

    const analytics::ObjectTypeDictionary objectTypeDictionary(
        q->commonModule()->analyticsObjectTypeDescriptorManager());

    for (const auto& packets: packetsBySource)
    {
        for (const auto& metadata: packets)
        {
            NX_ASSERT(metadata->metadataType == MetadataType::ObjectDetection);
            const auto compressedMetadata = std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
            const auto objectMetadata = nx::common::metadata::fromCompressedMetadataPacket(compressedMetadata);

            if (!objectMetadata || objectMetadata->objectMetadataList.empty())
                continue;

            for (const auto& item: objectMetadata->objectMetadataList)
            {
                const auto found = findObject(item.trackId);
                if (item.isBestShot())
                {
                    if (!found.track)
                        continue; //< A valid situation - an track can be filtered out.

                    if (item.objectMetadataType
                        == nx::common::metadata::ObjectMetadataType::externalBestShot)
                    {
                        m_externalBestShotTracks.insert(item.trackId);
                    }

                    found.track->bestShot.timestampUs = objectMetadata->timestampUs;
                    found.track->bestShot.rect = item.boundingBox;
                    found.track->bestShot.streamIndex  = objectMetadata->streamIndex;

                    switch (found.type)
                    {
                        case TrackType::beingAdded:
                            break;

                        case TrackType::loaded:
                            m_dataChangedTrackIds.insert(found.track->id);
                            m_emitDataChanged->requestOperation();
                            break;

                        case TrackType::noBestShot:
                        {
                            newObjectIndices[found.track->id] = int(newTracks.size());
                            newTracks.push_back(*found.track);
                            m_noBestShotTracks.erase(m_noBestShotTracks.begin() + found.index);
                            break;
                        }
                    }

                    continue;
                }

                ObjectPosition pos;
                pos.deviceId = objectMetadata->deviceId;
                pos.timestampUs = objectMetadata->timestampUs;
                pos.durationUs = objectMetadata->durationUs;
                pos.boundingBox = item.boundingBox;

                if (found.track)
                {
                    pos.attributes = item.attributes;
                    advanceTrack(*found.track, std::move(pos), found.type == TrackType::loaded);
                    continue;
                }

                ObjectTrack newTrack;
                newTrack.id = item.trackId;
                newTrack.deviceId = objectMetadata->deviceId;
                newTrack.objectTypeId = item.typeId;
                newTrack.attributes = item.attributes;
                newTrack.firstAppearanceTimeUs = objectMetadata->timestampUs;
                newTrack.lastAppearanceTimeUs = objectMetadata->timestampUs;
                newTrack.objectPosition.add(item.boundingBox);

                if (!filter.acceptsTrack(newTrack, objectTypeDictionary))
                    continue;

                newObjectIndices[item.trackId] = int(newTracks.size());
                newTracks.push_back(newTrack);
            }
        }
    }

    // Move new tracks without best shot to a dedicated queue.
    for (int i = 0; i < (int) newTracks.size(); ++i)
    {
        if (newTracks[i].bestShot.initialized())
            continue;

        m_noBestShotTracks.push_back(newTracks[i]);
        newTracks.erase(newTracks.begin() + i);
        --i;
    }

    // Limit the size of the no-best-shot track queue.
    const int noBestShotTrackCount = int(m_noBestShotTracks.size());
    if (noBestShotTrackCount > kMaxumumNoBestShotTrackCount)
    {
        m_noBestShotTracks.erase(m_noBestShotTracks.begin(),
            m_noBestShotTracks.begin() + (noBestShotTrackCount - kMaxumumNoBestShotTrackCount));
    }

    if (newTracks.empty())
        return;

    NX_VERBOSE(q, "Detected %1 new object tracks", newTracks.size());

    std::sort(newTracks.begin(), newTracks.end(),
        [](const ObjectTrack& left, const ObjectTrack& right)
        {
            return left.firstAppearanceTimeUs < right.firstAppearanceTimeUs;
        });

    ScopedLiveCommit liveCommit(q);
    int middleCount = 0;

    // Handle insertions in the middle.
    auto it = newTracks.begin();
    if (!m_data.empty())
    {
        const auto latestTime = startTime(m_data.front());
        for (; it != newTracks.cend(); ++it)
        {
            const auto timestamp = startTime(*it);
            if (timestamp >= latestTime)
                break;

            if (timestamp <= q->fetchedTimeWindow().startTime())
                continue;

            const auto position = std::lower_bound(m_data.cbegin(), m_data.cend(), timestamp,
                lowerBoundPredicate) - m_data.begin();

            ScopedInsertRows insertRows(q, position, position);
            NX_ASSERT(!m_objectTrackIdToTimestamp.contains(it->id));
            m_objectTrackIdToTimestamp[it->id] = startTime(*it);

            m_data.insert(m_data.begin() + position, std::move(*it));
            ++middleCount;
        }
    }

    // Handle insertion at the top.
    const int topCount = (int) (newTracks.end() - it);
    if (it != newTracks.end())
    {
        auto periodToCommit = QnTimePeriod::fromInterval(
            startTime(*it), startTime(newTracks.back()));

        q->addToFetchedTimeWindow(periodToCommit);

        NX_VERBOSE(q, "Live update commit");
        commitInternal(periodToCommit,
            std::make_reverse_iterator(newTracks.end()), std::make_reverse_iterator(it),
            /*position*/ 0, /*handleOverlaps*/ false);
    }

    NX_VERBOSE(q, "%1 new object tracks inserted in the middle, %2 at the top, %3 skipped as too old",
        middleCount, topCount, ((int) newTracks.size()) - (middleCount + topCount));

    if (count() > q->maximumCount())
    {
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
        const auto index = indexOf(id);
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
    // Currently there's a mess between track.attributes and track.track[i].attributes.
    // There's no clear understanding what to use and what to show.

    for (const auto& attribute: position.attributes)
    {
        const auto reverseIter = std::find_if(track.attributes.rbegin(), track.attributes.rend(),
            [&attribute](const nx::common::metadata::Attribute& value)
            {
                return attribute.name == value.name;
            });

        if (reverseIter == track.attributes.rend())
        {
            track.attributes.push_back(attribute);
            continue;
        }

        bool hasSameValue = false;
        for (auto iter = reverseIter; iter != track.attributes.rend(); ++iter)
        {
            if (iter->name != attribute.name)
                break;

            hasSameValue = iter->value == attribute.value;
            if (hasSameValue)
                break;
        }

        if (!hasSameValue)
            track.attributes.insert(reverseIter.base(), attribute);
    }

    track.lastAppearanceTimeUs = position.timestampUs;

    if (emitDataChanged)
    {
        m_dataChangedTrackIds.insert(track.id);
        m_emitDataChanged->requestOperation();
    }
}

int AnalyticsSearchListModel::Private::indexOf(const QnUuid& trackId) const
{
    const auto timestampIter = m_objectTrackIdToTimestamp.find(trackId);
    if (timestampIter == m_objectTrackIdToTimestamp.end())
        return -1;

    const auto range = std::make_pair(
        std::lower_bound(m_data.cbegin(), m_data.cend(), *timestampIter, lowerBoundPredicate),
        std::upper_bound(m_data.cbegin(), m_data.cend(), *timestampIter, upperBoundPredicate));

    const auto iter = std::find_if(range.first, range.second,
        [&trackId](const ObjectTrack& item) { return item.id == trackId; });

    return iter != range.second ? int(std::distance(m_data.cbegin(), iter)) : -1;
}

QString AnalyticsSearchListModel::Private::description(
    const ObjectTrack& track) const
{
    if (!ini().showDebugTimeInformationInRibbon)
        return QString();

    const auto timeWatcher = q->context()->instance<nx::vms::client::core::ServerTimeWatcher>();
    const auto start = timeWatcher->displayTime(startTime(track).count());
    const auto duration = objectDuration(track);

    using namespace std::chrono;
    return lm("Timestamp: %1 us<br>%2<br>Duration: %3 us<br>%4").args( //< Not translatable, debug.
        track.firstAppearanceTimeUs,
        start.toString(Qt::RFC2822Date),
        duration.count(),
        text::HumanReadable::timeSpan(duration_cast<milliseconds>(duration)));
}

QList<QPair<QString, QString>> AnalyticsSearchListModel::Private::attributes(
    const ObjectTrack& track) const
{
    if (track.attributes.empty())
        return {};

    const QString darkerColor = QPalette().color(QPalette::WindowText).name();

    const auto valuesText =
        [darkerColor](const auto beginIter, const auto endIter) -> QString
        {
            const auto count = (int) std::distance(beginIter, endIter);
            const auto effectiveEnd = beginIter + qMin(count, kMaxDisplayedAttributeValues);

            QString displayedAttributes = beginIter->value;

            for (auto iter = beginIter + 1; iter != effectiveEnd; ++iter)
                displayedAttributes += ", " + iter->value;

            if (count <= kMaxDisplayedAttributeValues)
                return displayedAttributes;

            return nx::format("%1 <font color=\"%3\">(%2)</font>", displayedAttributes,
                AnalyticsSearchListModel::tr("+%n values", "", count - kMaxDisplayedAttributeValues),
                darkerColor);
        };

    QList<QPair<QString, QString>> result;
    for (auto begin = track.attributes.cbegin(); begin != track.attributes.cend(); )
    {
        if (begin->name.isEmpty() || isAnalyticsAttributeHidden(begin->name))
        {
            ++begin;
            continue;
        }

        auto end = begin + 1;
        while (end != track.attributes.cend() && end->name == begin->name)
            ++end;

        result.push_back({begin->name, valuesText(begin, end)});
        begin = end;
    }

    return result;
}

QSharedPointer<QMenu> AnalyticsSearchListModel::Private::contextMenu(
    const ObjectTrack& track) const
{
    const auto camera = this->camera(track);
    if (!camera)
        return {};

    const nx::analytics::ActionTypeDescriptorManager descriptorManager(q->commonModule());
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
                    emit q->pluginActionRequested(engineId, actionId, track, camera, {});
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
