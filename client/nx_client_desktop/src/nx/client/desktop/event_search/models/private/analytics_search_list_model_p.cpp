#include "analytics_search_list_model_p.h"

#include <algorithm>
#include <chrono>

#include <QtGui/QPalette>
#include <QtWidgets/QMenu>

#include <analytics/common/object_detection_metadata.h>
#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <nx/client/core/watchers/server_time_watcher.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include <ini.h>
#include <nx/client/core/utils/human_readable.h>
#include <nx/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/client/desktop/utils/managed_camera_set.h>
#include <nx/utils/datetime.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/event/analytics_helper.h>

namespace nx::client::desktop {

using namespace analytics::storage;

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

static constexpr milliseconds kMetadataTimerInterval = 250ms;
static constexpr milliseconds kDataChangedInterval = 250ms;
static constexpr milliseconds kUpdateWorkbenchFilterDelay = 100ms;

milliseconds startTime(const DetectedObject& object)
{
    return duration_cast<milliseconds>(microseconds(object.firstAppearanceTimeUsec));
}

static const auto lowerBoundPredicate =
    [](const DetectedObject& left, milliseconds right)
    {
        return startTime(left) > right;
    };

static const auto upperBoundPredicate =
    [](milliseconds left, const DetectedObject& right)
    {
        return left > startTime(right);
    };

} // namespace

AnalyticsSearchListModel::Private::Private(AnalyticsSearchListModel* q):
    base_type(q),
    q(q),
    m_emitDataChanged(new utils::PendingOperation([this] { emitDataChangedIfNeeded(); },
        kDataChangedInterval.count(), this)),
    m_metadataProcessingTimer(new QTimer())
{
    m_emitDataChanged->setFlags(utils::PendingOperation::NoFlags);

    m_metadataProcessingTimer->setInterval(kMetadataTimerInterval.count());
    connect(m_metadataProcessingTimer.data(), &QTimer::timeout, this, &Private::processMetadata);
    m_metadataProcessingTimer->start();

    connect(q, &AbstractSearchListModel::camerasChanged, this, &Private::updateMetadataReceivers);
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
    const auto& object = m_data[index.row()];
    handled = true;

    static const auto kDefaultLocale = QString();
    switch (role)
    {
        case Qt::DisplayRole:
        {
            const auto name = vms::event::AnalyticsHelper::objectTypeName(
                camera(object), object.objectTypeId, kDefaultLocale);

            return name.isEmpty() ? tr("Unknown object") : name;
        }

        case Qt::DecorationRole:
            return QVariant::fromValue(qnSkin->pixmap(lit("text_buttons/analytics.png")));

        case Qn::DescriptionTextRole:
            return description(object);

        case Qn::AdditionalTextRole:
            return attributes(object);

        case Qn::TimestampRole:
            return object.firstAppearanceTimeUsec;

        case Qn::PreviewTimeRole:
            return previewParams(object).timestamp.count();

        case Qn::DurationRole:
        {
            using namespace std::chrono;
            const auto durationUs = object.lastAppearanceTimeUsec - object.firstAppearanceTimeUsec;
            return QVariant::fromValue(duration_cast<milliseconds>(microseconds(durationUs)).count());
        }

        case Qn::HelpTopicIdRole:
            return Qn::Empty_Help;

        case Qn::ResourceListRole:
            return QVariant::fromValue(QnResourceList({camera(object)}));

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(camera(object));

        case Qn::ItemZoomRectRole:
            return QVariant::fromValue(previewParams(object).boundingBox);

        case Qn::ContextMenuRole:
            return QVariant::fromValue(contextMenu(object));

        default:
            handled = false;
            return QVariant();
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
    if (m_filterText == value)
        return;

    q->clear();
    m_filterText = value;
}

void AnalyticsSearchListModel::Private::clearData()
{
    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_objectIdToTimestamp.clear();
}

void AnalyticsSearchListModel::Private::truncateToMaximumCount()
{
    const auto itemCleanup =
        [this](const DetectedObject& object) { m_objectIdToTimestamp.remove(object.objectId); };

    this->truncateDataToMaximumCount(m_data, &startTime, itemCleanup);
}

void AnalyticsSearchListModel::Private::truncateToRelevantTimePeriod()
{
    const auto itemCleanup =
        [this](const DetectedObject& object) { m_objectIdToTimestamp.remove(object.objectId); };

    this->truncateDataToTimePeriod(
        m_data, upperBoundPredicate, q->relevantTimePeriod(), itemCleanup);
}

bool AnalyticsSearchListModel::Private::hasAccessRights() const
{
    return q->accessController()->hasGlobalPermission(GlobalPermission::viewLogs);
}

rest::Handle AnalyticsSearchListModel::Private::requestPrefetch(const QnTimePeriod& period)
{
    const auto dataReceived =
        [this](bool success, rest::Handle requestId, LookupResult&& data)
        {
            if (!requestId || requestId != currentRequest().id)
                return;

            QnTimePeriod actuallyFetched;
            m_prefetch = analytics::storage::LookupResult();

            if (success)
            {
                m_prefetch = std::move(data);
                NX_ASSERT(m_prefetch.empty() || !m_prefetch.front().track.empty());

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
        const auto last = m_data.front();
        const auto lastTimeUs = last.firstAppearanceTimeUsec;

        while (end != begin)
        {
            const auto iter = end - 1;
            const auto timeUs = iter->firstAppearanceTimeUsec;

            if (timeUs > lastTimeUs)
                break;

            end = iter;

            if (timeUs == lastTimeUs && iter->objectId == last.objectId)
                break;
        }
    }

    const auto count = std::distance(begin, end);
    if (count <= 0)
    {
        NX_VERBOSE(q) << "Committing no analytics";
        return false;
    }

    NX_VERBOSE(q) << "Committing" << count << "analytics from"
        << utils::timestampToDebugString(startTime(*(end - 1)).count()) << "to"
        << utils::timestampToDebugString(startTime(*begin).count());

    ScopedInsertRows insertRows(q, position, position + count - 1);
    for (auto iter = begin; iter != end; ++iter)
    {
        if (!m_objectIdToTimestamp.contains(iter->objectId)) //< Just to be safe.
            m_objectIdToTimestamp[iter->objectId] = startTime(*iter);
    }

    m_data.insert(m_data.begin() + position,
        std::make_move_iterator(begin), std::make_move_iterator(end));

    return true;
}

bool AnalyticsSearchListModel::Private::commitPrefetch(const QnTimePeriod& periodToCommit)
{
    const auto clearPrefetch = nx::utils::makeScopeGuard([this]() { m_prefetch.clear(); });

    if (currentRequest().direction == FetchDirection::earlier)
        return commitInternal(periodToCommit, m_prefetch.cbegin(), m_prefetch.cend(), count(), false);

    NX_ASSERT(currentRequest().direction == FetchDirection::later);
    return commitInternal(
        periodToCommit, m_prefetch.crbegin(), m_prefetch.crend(), 0, q->effectiveLiveSupported());
}

rest::Handle AnalyticsSearchListModel::Private::getObjects(const QnTimePeriod& period,
    GetCallback callback, int limit)
{
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return false;

    Filter request;
    if (q->cameraSet()->type() != ManagedCameraSet::Type::all)
    {
        const auto cameras = q->cameraSet()->cameras();
        request.deviceIds.reserve(cameras.size());
        std::transform(cameras.cbegin(), cameras.cend(), std::back_inserter(request.deviceIds),
            [](const QnVirtualCameraResourcePtr& camera) { return camera->getId(); });
    }

    request.timePeriod = period;
    request.maxObjectsToSelect = limit;
    request.freeText = m_filterText;

    request.boundingBox = q->cameraSet()->type() == ManagedCameraSet::Type::single
        ? m_filterRect
        : QRectF();

    request.sortOrder = currentRequest().direction == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    NX_VERBOSE(q) << "Requesting analytics from"
        << utils::timestampToDebugString(period.startTimeMs) << "to"
        << utils::timestampToDebugString(period.endTimeMs()) << "in"
        << QVariant::fromValue(request.sortOrder).toString()
        << "maximum objects" << request.maxObjectsToSelect;

    const auto internalCallback =
        [callback, guard = QPointer<Private>(this)](
            bool success, rest::Handle handle, LookupResult&& data)
        {
            if (guard)
                callback(success, handle, std::move(data));
        };

    return server->restConnection()->lookupDetectedObjects(
        request, false /*isLocal*/, internalCallback, thread());
}

void AnalyticsSearchListModel::Private::updateMetadataReceivers()
{
    auto cameras = q->cameras();
    decltype(m_metadataReceivers) newMetadataReceivers;

    // Preserve existing receivers that are still relevant.
    for (auto& receiver: m_metadataReceivers)
    {
        if (cameras.remove(receiver->camera()))
            newMetadataReceivers.emplace_back(receiver.release());
    }

    // Create new receivers if needed.
    for (const auto& camera: cameras)
        newMetadataReceivers.emplace_back(new LiveAnalyticsReceiver(camera));

    m_metadataReceivers = std::move(newMetadataReceivers);
}

void AnalyticsSearchListModel::Private::processMetadata()
{
    QList<DetectedObject> newObjects;
    QHash<QnUuid, int> newObjectIndices;
    int newObjectSources = 0;

    for (const auto& receiver: m_metadataReceivers)
    {
        auto packets = receiver->takeData();
        if (!q->isLive() || packets.empty())
            continue;

        NX_VERBOSE(this) << "Processing" << packets.size() << "live metadata packets";

        for (const auto& metadata: packets)
        {
            NX_ASSERT(metadata->metadataType == MetadataType::ObjectDetection);
            const auto compressedMetadata = std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
            const auto detectionMetadata = common::metadata::fromMetadataPacket(compressedMetadata);

            if (!detectionMetadata || detectionMetadata->objects.empty())
                continue;

            ObjectPosition pos;
            pos.deviceId = detectionMetadata->deviceId;
            pos.timestampUsec = detectionMetadata->timestampUsec;
            pos.durationUsec = detectionMetadata->durationUsec;

            for (const auto& item: detectionMetadata->objects)
            {
                pos.boundingBox = item.boundingBox;

                auto index = newObjectIndices.value(item.objectId, -1);
                if (index >= 0)
                {
                    pos.attributes = std::move(item.labels);
                    advanceObject(newObjects[index], std::move(pos), false);
                    continue;
                }

                index = indexOf(item.objectId);
                if (index >= 0)
                {
                    pos.attributes = std::move(item.labels);
                    advanceObject(m_data[index], std::move(pos));
                    continue;
                }

                if (m_filterRect.isValid() && !m_filterRect.intersects(item.boundingBox))
                    continue;

                DetectedObject newObject;
                newObject.objectId = item.objectId;
                newObject.objectTypeId = item.objectTypeId;
                newObject.attributes = std::move(item.labels);
                newObject.track.push_back(pos);
                newObject.firstAppearanceTimeUsec = pos.timestampUsec;
                newObject.lastAppearanceTimeUsec = pos.timestampUsec;

                newObjectIndices[item.objectId] = newObjects.size();
                newObjects.push_back(std::move(newObject));
                ++newObjectSources;
            }
        }
    }

    if (newObjects.empty())
        return;

    if (newObjectSources > 1)
    {
        std::sort(newObjects.begin(), newObjects.end(),
            [](const DetectedObject& left, const DetectedObject& right)
            {
                return left.firstAppearanceTimeUsec < right.firstAppearanceTimeUsec;
            });
    }

    ScopedInsertRows insertRows(q, 0, int(newObjects.size()) - 1);

    for (const auto& newObject: newObjects)
        m_objectIdToTimestamp[newObject.objectId] = startTime(newObject);

    m_data.insert(m_data.begin(),
        std::make_move_iterator(newObjects.begin()),
        std::make_move_iterator(newObjects.end()));
}

void AnalyticsSearchListModel::Private::emitDataChangedIfNeeded()
{
    if (m_dataChangedObjectIds.empty())
        return;

    for (const auto& id: m_dataChangedObjectIds)
    {
        const auto index = indexOf(id);
        if (index < 0)
            continue;

        const auto modelIndex = q->index(index);
        emit q->dataChanged(modelIndex, modelIndex);
    }

    m_dataChangedObjectIds.clear();
};

void AnalyticsSearchListModel::Private::advanceObject(DetectedObject& object,
    ObjectPosition&& position, bool emitDataChanged)
{
    // Currently there's a mess between object.attributes and object.track[i].attributes.
    // There's no clear understanding what to use and what to show.
    // On GUI side we use just object.attributes for now.

    for (const auto& attribute: position.attributes)
    {
        auto iter = std::find_if(
            object.attributes.begin(),
            object.attributes.end(),
            [&attribute](const common::metadata::Attribute& value)
            {
                return attribute.name == value.name;
            });

        if (iter != object.attributes.end())
            iter->value = attribute.value;
        else
            object.attributes.push_back(attribute);
    }

    object.lastAppearanceTimeUsec = position.timestampUsec;
    m_dataChangedObjectIds.insert(object.objectId);

    if (emitDataChanged)
        m_emitDataChanged->requestOperation();
}

int AnalyticsSearchListModel::Private::indexOf(const QnUuid& objectId) const
{
    const auto timestampIter = m_objectIdToTimestamp.find(objectId);
    if (timestampIter == m_objectIdToTimestamp.end())
        return -1;

    const auto range = std::make_pair(
        std::lower_bound(m_data.cbegin(), m_data.cend(), *timestampIter, lowerBoundPredicate),
        std::upper_bound(m_data.cbegin(), m_data.cend(), *timestampIter, upperBoundPredicate));

    const auto iter = std::find_if(range.first, range.second,
        [&objectId](const DetectedObject& item) { return item.objectId == objectId; });

    return iter != range.second ? int(std::distance(m_data.cbegin(), iter)) : -1;
}

QString AnalyticsSearchListModel::Private::description(
    const DetectedObject& object) const
{
    if (!ini().showDebugTimeInformationInRibbon)
        return QString();

    const auto timeWatcher = q->context()->instance<nx::client::core::ServerTimeWatcher>();
    const auto start = timeWatcher->displayTime(startTime(object).count());
    // TODO: #vkutin Is this duration formula good enough for us?
    //   Or we need to add some "lastAppearanceDurationUsec"?
    const auto durationUs = object.lastAppearanceTimeUsec - object.firstAppearanceTimeUsec;

    using namespace std::chrono;
    return lit("Timestamp: %1 us<br>%2<br>Duration: %3 us<br>%4")
        .arg(object.firstAppearanceTimeUsec)
        .arg(start.toString(Qt::RFC2822Date))
        .arg(durationUs)
        .arg(core::HumanReadable::timeSpan(duration_cast<milliseconds>(microseconds(durationUs))));
}

QString AnalyticsSearchListModel::Private::attributes(
    const DetectedObject& object) const
{
    if (object.attributes.empty())
        return QString();

    static const auto kCss = QString::fromLatin1(R"(
            <style type = 'text/css'>
                th { color: %1; font-weight: normal; text-align: left; }
            </style>)");

    static const auto kTableTemplate = lit("<table cellpadding='0' cellspacing='0'>%1</table>");
    static const auto kRowTemplate = lit("<tr><th>%1</th>")
        + lit("<td width='%1'/>").arg(style::Metrics::kStandardPadding) //< Spacing.
        + lit("<td>%2</td></tr>");

    QString rows;
    for (const auto& attribute: object.attributes)
    {
        if (!attribute.name.startsWith(lit("nx.sys.")))
            rows += kRowTemplate.arg(attribute.name, attribute.value);
    }

    if (rows.isEmpty())
        return QString();

    const auto color = QPalette().color(QPalette::WindowText);
    return kCss.arg(color.name()) + kTableTemplate.arg(rows);
}

QSharedPointer<QMenu> AnalyticsSearchListModel::Private::contextMenu(
    const analytics::storage::DetectedObject& object) const
{
    const auto camera = this->camera(object);
    if (!camera)
        return QSharedPointer<QMenu>();

    // TODO: #vkutin Is this a correct way of choosing servers for analytics actions?
    auto servers = q->cameraHistoryPool()->getCameraFootageData(camera, true);
    servers.push_back(camera->getParentServer());

    const auto allActions = vms::event::AnalyticsHelper::availableActions(
        servers, object.objectTypeId);
    if (allActions.isEmpty())
        return QSharedPointer<QMenu>();

    QSharedPointer<QMenu> menu(new QMenu());
    for (const auto& driverActions: allActions)
    {
        if (!menu->isEmpty())
            menu->addSeparator();

        const auto& pluginId = driverActions.pluginId;
        for (const auto& action: driverActions.actions)
        {
            const auto name = action.name.text(QString());
            menu->addAction(name,
                [this, action, object, pluginId, guard = QPointer<const Private>(this)]()
                {
                    if (guard)
                        executePluginAction(pluginId, action, object);
                });
        }
    }

    return menu;
}

void AnalyticsSearchListModel::Private::executePluginAction(
    const QString& pluginId,
    const nx::vms::api::analytics::EngineManifest::ObjectAction& action,
    const analytics::storage::DetectedObject& object) const
{
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return;

    const auto resultCallback =
        [this, guard = QPointer<const Private>(this)](
            bool success, rest::Handle /*requestId*/, QnJsonRestResult result)
        {
            if (result.error != QnRestResult::NoError)
            {
                QnMessageBox::warning(q->mainWindowWidget(), tr("Failed to execute plugin action"),
                    result.errorString);
                return;
            }

            if (!success)
                return;

            const auto reply = result.deserialized<AnalyticsActionResult>();
            if (!reply.messageToUser.isEmpty())
                QnMessageBox::success(q->mainWindowWidget(), reply.messageToUser);

            if (!reply.actionUrl.isEmpty())
                WebViewDialog::showUrl(QUrl(reply.actionUrl));
        };

    AnalyticsAction actionData;
    actionData.pluginId = pluginId;
    actionData.actionId = action.id;
    actionData.objectId = object.objectId;

    server->restConnection()->executeAnalyticsAction(actionData, resultCallback, thread());
}

AnalyticsSearchListModel::Private::PreviewParams AnalyticsSearchListModel::Private::previewParams(
    const analytics::storage::DetectedObject& object)
{
    PreviewParams result;
    result.timestamp = std::chrono::microseconds(object.firstAppearanceTimeUsec);
    result.boundingBox = object.track.empty()
        ? QRectF()
        : object.track.front().boundingBox;

    const auto attribute =
        [&object](const QString& name)
        {
            const auto iter = std::find_if(object.attributes.cbegin(), object.attributes.cend(),
                [&name](const common::metadata::Attribute& attribute)
                {
                    return attribute.name == name;
                });

            return (iter != object.attributes.cend()) ? iter->value : QString();
        };

    const auto getRealAttribute =
        [&attribute](const QString& name, qreal defaultValue)
        {
            bool ok = false;
            QString valueStr = attribute(name);
            if (valueStr.isNull())
                return defaultValue; //< Attribute is missing.

            valueStr.replace(',', '.'); //< Protection against localized decimal point.
            const qreal value = valueStr.toDouble(&ok);
            if (ok)
                return value;

            NX_WARNING(typeid(Private)) << lm("Invalid %1 value: \"%2\"").args(name, valueStr);
            return defaultValue;
        };

    const QString previewTimestampStr = attribute(lit("nx.sys.preview.timestampUs"));
    if (!previewTimestampStr.isNull())
    {
        const qint64 previewTimestampUs = previewTimestampStr.toLongLong();
        if (previewTimestampUs > 0)
        {
            result.timestamp = std::chrono::microseconds(previewTimestampUs);
        }
        else
        {
            NX_WARNING(typeid(Private)) << lm("Invalid nx.sys.preview.timestampUs value: \"%1\"")
                .arg(previewTimestampStr);
        }
    }

    result.boundingBox.setLeft(getRealAttribute(lit("nx.sys.preview.boundingBox.x"),
        result.boundingBox.left()));

    result.boundingBox.setTop(getRealAttribute(lit("nx.sys.preview.boundingBox.y"),
        result.boundingBox.top()));

    result.boundingBox.setWidth(getRealAttribute(lit("nx.sys.preview.boundingBox.width"),
        result.boundingBox.width()));

    result.boundingBox.setHeight(getRealAttribute(lit("nx.sys.preview.boundingBox.height"),
        result.boundingBox.height()));

    return result;
}

QnVirtualCameraResourcePtr AnalyticsSearchListModel::Private::camera(
    const analytics::storage::DetectedObject& object) const
{
    NX_ASSERT(!object.track.empty());
    if (object.track.empty())
        return {};

    if (q->cameras().size() == 1) //< An optimization.
    {
        const auto result = *q->cameras().cbegin();
        NX_ASSERT(result->getId() == object.track[0].deviceId);
        return result;
    }

    return q->resourcePool()->getResourceById<QnVirtualCameraResource>(object.track[0].deviceId);
}

} // namespace nx::client::desktop
