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
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/utils/datetime.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/range_adapters.h>

namespace nx::vms::client::desktop {

using namespace analytics::db;

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

static constexpr milliseconds kMetadataTimerInterval = 1000ms;
static constexpr milliseconds kDataChangedInterval = 500ms;
static constexpr milliseconds kUpdateWorkbenchFilterDelay = 100ms;

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

    auto serverChangesListener = new QnResourceChangesListener(q);
    serverChangesListener->connectToResources<QnVirtualCameraResource>(&QnResource::statusChanged,
        [this](const QnResourcePtr& resource)
        {
            if (!this->q->isOnline())
                return;

            if (isCameraApplicable(resource.dynamicCast<QnVirtualCameraResource>()))
                updateMetadataReceivers();
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
    const auto& track = m_data[index.row()];
    handled = true;

    static const auto kDefaultLocale = QString();
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

        case Qt::DecorationRole:
            return QVariant::fromValue(qnSkin->pixmap("analytics/analytics.svg"));

        case Qn::DescriptionTextRole:
            return description(track);

        case Qn::AdditionalTextRole:
            return attributes(track);

        case Qn::TimestampRole:
            return QVariant::fromValue(std::chrono::microseconds(track.firstAppearanceTimeUs));

        case Qn::PreviewTimeRole:
            return QVariant::fromValue(previewParams(track).timestamp);

        case Qn::PreviewStreamSelectionRole:
            return QVariant::fromValue(
                nx::api::CameraImageRequest::StreamSelectionMode::sameAsAnalytics);

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
    if (Filter::userInputToFreeText(m_filterText) == Filter::userInputToFreeText(value))
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
}

void AnalyticsSearchListModel::Private::truncateToMaximumCount()
{
    const auto itemCleanup =
        [this](const ObjectTrack& track)
        {
            m_objectTrackIdToTimestamp.remove(track.id);
        };

    q->truncateDataToMaximumCount(m_data, &startTime, itemCleanup);
}

void AnalyticsSearchListModel::Private::truncateToRelevantTimePeriod()
{
    const auto itemCleanup =
        [this](const ObjectTrack& track)
        {
            m_objectTrackIdToTimestamp.remove(track.id);
        };

    q->truncateDataToTimePeriod(
        m_data, &startTime, q->relevantTimePeriod(), itemCleanup);
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
            m_prefetch = analytics::db::LookupResult();

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
    request.loadUserInputToFreeText(m_filterText);

    if (!m_selectedObjectType.isEmpty())
        request.objectTypeId = {m_selectedObjectType};

    if (q->cameraSet()->type() == ManagedCameraSet::Type::single && m_filterRect.isValid())
        request.boundingBox = m_filterRect;

    request.sortOrder = currentRequest().direction == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    NX_VERBOSE(q, "Requesting analytics:\n    from: %1\n    to: %2\n"
        "    box: %3\n    text filter: %4\n    sort: %5\n    limit: %6",
        nx::utils::timestampToDebugString(period.startTimeMs),
        nx::utils::timestampToDebugString(period.endTimeMs()),
        request.boundingBox,
        request.freeText,
        QVariant::fromValue(request.sortOrder).toString(),
        request.maxObjectTracksToSelect);

    return server->restConnection()->lookupObjectTracks(
        request, false /*isLocal*/, nx::utils::guarded(this, callback), thread());
}

void AnalyticsSearchListModel::Private::updateMetadataReceivers()
{
    if (m_liveReceptionActive)
    {
        auto cameras = q->cameras();
        MetadataReceiverList newMetadataReceivers;

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

    for (const auto& receiver: m_metadataReceivers)
    {
        auto packets = receiver->takeData();
        if (packets.empty())
            continue;

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

    struct FoundObjectTrack
    {
        ObjectTrack* const track = nullptr;
        const bool isNew = false;
    };

    const auto findObject =
        [&](const QnUuid& trackId) -> FoundObjectTrack
        {
            auto index = newObjectIndices.value(trackId, -1);
            if (index >= 0)
                return {&newTracks[index], true};

            index = indexOf(trackId);
            if (index >= 0)
                return {&m_data[index], false};

            return {};
        };

    nx::analytics::db::Filter filter;
    filter.loadUserInputToFreeText(m_filterText);
    if (m_filterRect.isValid())
        filter.boundingBox = m_filterRect;
    if (!m_selectedObjectType.isEmpty())
        filter.objectTypeId.push_back(m_selectedObjectType);

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
                if (item.bestShot)
                {
                    if (!found.track)
                        continue; //< A valid situation - an track can be filtered out.

                    found.track->bestShot.timestampUs = objectMetadata->timestampUs;
                    found.track->bestShot.rect = item.boundingBox;

                    if (found.isNew)
                        continue;

                    m_dataChangedTrackIds.insert(found.track->id);
                    m_emitDataChanged->requestOperation();
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
                    advanceTrack(*found.track, std::move(pos), !found.isNew);
                    continue;
                }

                if (!filter.acceptsMetadata(item))
                    continue;

                ObjectTrack newTrack;
                newTrack.id = item.trackId;
                newTrack.deviceId = objectMetadata->deviceId;
                newTrack.objectTypeId = item.typeId;
                newTrack.attributes = item.attributes;
                newTrack.bestShot.rect = item.boundingBox;
                newTrack.bestShot.timestampUs = objectMetadata->timestampUs;
                newTrack.firstAppearanceTimeUs = objectMetadata->timestampUs;
                newTrack.lastAppearanceTimeUs = objectMetadata->timestampUs;

                newObjectIndices[item.trackId] = int(newTracks.size());
                newTracks.push_back(newTrack);
            }
        }
    }

    if (newTracks.empty())
        return;

    NX_VERBOSE(q, "Detected %1 new object tracks", newTracks.size());

    if (packetsBySource.size() > 1)
    {
        std::sort(newTracks.begin(), newTracks.end(),
            [](const ObjectTrack& left, const ObjectTrack& right)
            {
                return left.firstAppearanceTimeUs < right.firstAppearanceTimeUs;
            });
    }

    auto periodToCommit = QnTimePeriod::fromInterval(
        startTime(newTracks.front()), startTime(newTracks.back()));

    ScopedLiveCommit liveCommit(q);

    q->addToFetchedTimeWindow(periodToCommit);

    NX_VERBOSE(q, "Live update commit");
    commitInternal(periodToCommit, newTracks.rbegin(), newTracks.rend(), 0, true);

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
    // On GUI side we use just track.attributes for now.

    for (const auto& attribute: position.attributes)
    {
        auto iter = std::find_if(
            track.attributes.begin(),
            track.attributes.end(),
            [&attribute](const nx::common::metadata::Attribute& value)
            {
                return attribute.name == value.name;
            });

        if (iter != track.attributes.end())
            iter->value = attribute.value;
        else
            track.attributes.push_back(attribute);
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

QString AnalyticsSearchListModel::Private::attributes(
    const ObjectTrack& track) const
{
    if (track.attributes.empty())
        return QString();

    static const auto kCss = QString::fromLatin1(R"(
            <style type = 'text/css'>
                th { color: %1; font-weight: normal; text-align: left; }
            </style>)");

    static const auto kTableTemplate = QString("<table cellpadding='0' cellspacing='0'>%1</table>");
    static const auto kRowTemplate = QString("<tr><th>%1</th>")
        + QString("<td width='%1'/>").arg(style::Metrics::kStandardPadding) //< Spacing.
        + QString("<td>%2</td></tr>");

    QString rows;
    for (const auto& attribute: track.attributes)
    {
        if (!attribute.name.startsWith("nx.sys."))
            rows += kRowTemplate.arg(attribute.name, attribute.value);
    }

    if (rows.isEmpty())
        return QString();

    const auto color = QPalette().color(QPalette::WindowText);
    return kCss.arg(color.name()) + kTableTemplate.arg(rows);
}

QSharedPointer<QMenu> AnalyticsSearchListModel::Private::contextMenu(
    const analytics::db::ObjectTrack& track) const
{
    using nx::vms::api::analytics::ActionTypeDescriptor;
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
    const analytics::db::ObjectTrack& track)
{
    if (track.bestShot.initialized())
        return {microseconds(track.bestShot.timestampUs), track.bestShot.rect};

    const auto rect = track.bestShot.rect;
    return {microseconds(track.firstAppearanceTimeUs), rect};
}

QnVirtualCameraResourcePtr AnalyticsSearchListModel::Private::camera(
    const analytics::db::ObjectTrack& track) const
{
    const auto& deviceId = track.deviceId;
    if (NX_ASSERT(!deviceId.isNull()))
        return q->resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId);

    // Fallback mechanism, just in case.

    return q->resourcePool()->getResourceById<QnVirtualCameraResource>(
        track.deviceId);
}

} // namespace nx::vms::client::desktop
