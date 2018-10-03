#include "analytics_search_list_model_p.h"

#include <chrono>

#include <QtGui/QPalette>
#include <QtWidgets/QMenu>

#include <analytics/common/object_detection_metadata.h>
#include <api/server_rest_connection.h>
#include <camera/resource_display.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <nx/client/core/watchers/server_time_watcher.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include <ini.h>
#include <nx/client/core/utils/human_readable.h>
#include <nx/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/utils/datetime.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/event/analytics_helper.h>

namespace nx {
namespace client {
namespace desktop {

using namespace analytics::storage;

namespace {

static constexpr int kFetchBatchSize = 110;

using namespace std::chrono;
using namespace std::chrono_literals;

static constexpr milliseconds kUpdateTimerInterval = 30s;
static constexpr milliseconds kMetadataTimerInterval = 10ms;
static constexpr milliseconds kDataChangedInterval = 250ms;

static const auto lowerBoundPredicateUs =
    [](const DetectedObject& left, qint64 rightUs)
    {
        return left.firstAppearanceTimeUsec > rightUs;
    };

static const auto upperBoundPredicateUs =
    [](qint64 leftUs, const DetectedObject& right)
    {
        return leftUs > right.firstAppearanceTimeUsec;
    };

static const auto upperBoundPredicateMs =
    [](qint64 leftMs, const DetectedObject& right)
    {
        return leftMs > duration_cast<milliseconds>(
            microseconds(right.firstAppearanceTimeUsec)).count();
    };

} // namespace

AnalyticsSearchListModel::Private::Private(AnalyticsSearchListModel* q):
    base_type(q),
    q(q),
    m_updateTimer(new QTimer()),
    m_emitDataChanged(new utils::PendingOperation([this] { emitDataChangedIfNeeded(); },
        kDataChangedInterval.count(), this)),
    m_updateWorkbenchFilter(createUpdateWorkbenchFilterOperation()),
    m_metadataSource(createMetadataSource()),
    m_metadataProcessingTimer(new QTimer())
{
    m_emitDataChanged->setFlags(utils::PendingOperation::NoFlags);

    m_updateTimer->setInterval(kUpdateTimerInterval.count());
    connect(m_updateTimer.data(), &QTimer::timeout, this, &Private::periodicUpdate);

    m_metadataProcessingTimer->setInterval(kMetadataTimerInterval.count());
    connect(m_metadataProcessingTimer.data(), &QTimer::timeout, this, &Private::processMetadata);
    m_metadataProcessingTimer->start();
}

AnalyticsSearchListModel::Private::~Private()
{
}

utils::PendingOperation* AnalyticsSearchListModel::Private::createUpdateWorkbenchFilterOperation()
{
    const auto updateWorkbenchFilter =
        [this]()
        {
            if (!camera())
                return;

            Filter filter;
            filter.deviceIds.push_back(camera()->getId());
            filter.boundingBox = m_filterRect;
            filter.freeText = m_filterText;
            q->navigator()->setAnalyticsFilter(filter);
        };

    static constexpr int kUpdateWorkbenchFilterDelayMs = 100;

    auto result = new utils::PendingOperation(updateWorkbenchFilter,
        kUpdateWorkbenchFilterDelayMs, this);

    result->setFlags(utils::PendingOperation::FireOnlyWhenIdle);
    return result;
}

media::SignalingMetadataConsumer* AnalyticsSearchListModel::Private::createMetadataSource()
{
    const auto processMetadataInSourceThread =
        [this](const QnAbstractCompressedMetadataPtr& packet)
        {
            QnMutexLocker lock(&m_metadataMutex);
            m_metadataPackets.push_back(packet);
        };

    auto metadataSource = new media::SignalingMetadataConsumer(MetadataType::ObjectDetection);
    connect(metadataSource, &media::SignalingMetadataConsumer::metadataReceived,
        this, processMetadataInSourceThread, Qt::DirectConnection);

    return metadataSource;
}

void AnalyticsSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (camera == this->camera())
        return;

    base_type::setCamera(camera);
    refreshUpdateTimer();

    if (m_display)
        m_display->removeMetadataConsumer(m_metadataSource);

    m_display.reset();

    if (!camera)
        return;

    auto widget = qobject_cast<QnMediaResourceWidget*>(q->navigator()->currentWidget());
    NX_ASSERT(widget && widget->resource() == camera);

    if (!widget)
        return;

    m_display = widget->display();
    NX_ASSERT(m_display);

    if (m_display)
        m_display->addMetadataConsumer(m_metadataSource);
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
                camera(), object.objectTypeId, kDefaultLocale);

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
            return previewParams(object).timestampUs;

        case Qn::DurationRole:
        {
            using namespace std::chrono;
            const auto durationUs = object.lastAppearanceTimeUsec - object.firstAppearanceTimeUsec;
            return QVariant::fromValue(duration_cast<milliseconds>(microseconds(durationUs)).count());
        }

        case Qn::HelpTopicIdRole:
            return Qn::Empty_Help;

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(camera());

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

    clear();
    m_filterRect = relativeRect;
    m_updateWorkbenchFilter->requestOperation();
}

QString AnalyticsSearchListModel::Private::filterText() const
{
    return m_filterText;
}

void AnalyticsSearchListModel::Private::setFilterText(const QString& value)
{
    if (m_filterText == value)
        return;

    clear();
    m_filterText = value;
    m_updateWorkbenchFilter->requestOperation();
}

void AnalyticsSearchListModel::Private::clear()
{
    qDebug() << "Clear analytics model";

    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_objectIdToTimestampUs.clear();
    m_currentUpdateId = rest::Handle();
    m_latestTimeMs = qMin(qnSyncTime->currentMSecsSinceEpoch(), q->relevantTimePeriod().endTimeMs());
    m_filterRect = QRectF();
    base_type::clear();

    refreshUpdateTimer();
    m_updateWorkbenchFilter->requestOperation();
}

bool AnalyticsSearchListModel::Private::hasAccessRights() const
{
    return q->accessController()->hasGlobalPermission(GlobalPermission::viewLogs);
}

rest::Handle AnalyticsSearchListModel::Private::requestPrefetch(qint64 fromMs, qint64 toMs)
{
    const auto dataReceived =
        [this](bool success, rest::Handle handle, LookupResult&& data)
        {
            if (shouldSkipResponse(handle))
                return;

            m_prefetch = success ? std::move(data) : LookupResult();
            m_success = success;

            if (m_prefetch.empty())
            {
                qDebug() << "Pre-fetched no analytics";
            }
            else
            {
                qDebug() << "Pre-fetched" << m_prefetch.size() << "analytics from"
                    << utils::timestampToRfc2822(startTimeMs(m_prefetch.back())) << "to"
                    << utils::timestampToRfc2822(startTimeMs(m_prefetch.front()));
            }

            NX_ASSERT(m_prefetch.empty() || !m_prefetch.front().track.empty());
            complete(m_prefetch.size() >= kFetchBatchSize
                ? startTimeMs(m_prefetch.back()) + 1 /*discard last ms*/
                : 0);
        };

    qDebug() << "Requesting analytics from" << utils::timestampToRfc2822(fromMs)
        << "to" << utils::timestampToRfc2822(toMs);

    return getObjects(fromMs, toMs, dataReceived, kFetchBatchSize);
}

bool AnalyticsSearchListModel::Private::commitPrefetch(qint64 earliestTimeToCommitMs, bool& fetchedAll)
{
    if (!m_success)
    {
        qDebug() << "Committing no analytics";
        return false;
    }

    const auto latestTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(earliestTimeToCommitMs)).count();

    const auto end = std::upper_bound(m_prefetch.begin(), m_prefetch.end(),
        latestTimeUs, upperBoundPredicateUs);

    const auto first = this->count();
    const auto count = std::distance(m_prefetch.begin(), end);

    if (count > 0)
    {
        qDebug() << "Committing" << count << "analytics from"
            << utils::timestampToRfc2822(startTimeMs(m_prefetch[count - 1])) << "to"
            << utils::timestampToRfc2822(startTimeMs(m_prefetch.front()));

        ScopedInsertRows insertRows(q,  first, first + count - 1);

        for (auto iter = m_prefetch.begin(); iter != end; ++iter)
        {
            if (!m_objectIdToTimestampUs.contains(iter->objectId)) //< Just to be safe.
                m_objectIdToTimestampUs[iter->objectId] = iter->firstAppearanceTimeUsec;
        }

        m_data.insert(m_data.end(),
            std::make_move_iterator(m_prefetch.begin()),
            std::make_move_iterator(end));
    }
    else
    {
        qDebug() << "Committing no analytics";
    }

    fetchedAll = m_prefetch.empty();
    m_prefetch.clear();
    return true;
}

void AnalyticsSearchListModel::Private::clipToSelectedTimePeriod()
{
    m_currentUpdateId = rest::Handle(); //< Cancel timed update.
    m_latestTimeMs = qMin(m_latestTimeMs, q->relevantTimePeriod().endTimeMs());
    refreshUpdateTimer();

    const auto cleanupFunction =
        [this](const DetectedObject& object) { m_objectIdToTimestampUs.remove(object.objectId); };

    // Explicit specialization is required for gcc 4.6.
    clipToTimePeriod<decltype(m_data), decltype(upperBoundPredicateMs)>(
        m_data, upperBoundPredicateMs, q->relevantTimePeriod(), cleanupFunction);
}

void AnalyticsSearchListModel::Private::refreshUpdateTimer()
{
    if (camera() && q->relevantTimePeriod().isInfinite())
    {
        if (!m_updateTimer->isActive())
        {
            qDebug() << "Analytics search update timer started";
            m_updateTimer->start();
        }
    }
    else
    {
        if (m_updateTimer->isActive())
        {
            qDebug() << "Analytics search update timer stopped";
            m_updateTimer->stop();
        }
    }
}

void AnalyticsSearchListModel::Private::periodicUpdate()
{
    if (m_currentUpdateId)
        return;

    const auto eventsReceived =
        [this](bool success, rest::Handle handle, LookupResult&& data)
        {
            if (handle != m_currentUpdateId)
                return;

            m_currentUpdateId = rest::Handle();

            if (success && !data.empty())
                addNewlyReceivedObjects(std::move(data));
            else
                qDebug() << "Periodic update: no new objects added";
        };

    qDebug() << "Periodic update: requesting new analytics from"
        << utils::timestampToRfc2822(m_latestTimeMs) << "to infinity";

    m_currentUpdateId = getObjects(m_latestTimeMs,
        std::numeric_limits<qint64>::max(), eventsReceived);
}

void AnalyticsSearchListModel::Private::addNewlyReceivedObjects(
    LookupResult&& data)
{
    using namespace std::chrono;
    const auto latestTimeUs = duration_cast<microseconds>(milliseconds(m_latestTimeMs)).count();

    const auto overlapBegin = std::lower_bound(data.begin(), data.end(),
        latestTimeUs, lowerBoundPredicateUs);

    auto added = std::distance(data.begin(), overlapBegin);
    int updated = 0;

    for (auto iter = overlapBegin; iter != data.end(); ++iter)
    {
        const auto index = indexOf(iter->objectId);
        if (index < 0)
        {
            if (iter->firstAppearanceTimeUsec != latestTimeUs)
            {
                // A new object with timestamp older than latestTimeUs is invalid:
                //   we shouldn't insert objects in the middle of already loaded set.
                // TODO: #vkutin I'm not sure if it's a normal scenario or an assert is required.
                iter->firstAppearanceTimeUsec = 0; //< Mark to skip.
                continue;
            }

            // A new object with latestTimeUs timestamp is perfectly valid.
            ++added;
            continue;
        }

        // An existing object should be simply updated.
        ObjectPosition pos;
        pos.attributes = std::move(iter->attributes);
        pos.timestampUsec = iter->lastAppearanceTimeUsec;
        advanceObject(m_data[index], std::move(pos));
        iter->firstAppearanceTimeUsec = 0; //< Mark to skip.
    }

    qDebug() << "Periodic update:" << added << "new objects added," << updated << "objects updated";

    if (added == 0)
        return;

    {
        ScopedInsertRows insertRows(q, 0, added - 1);
        for (auto iter = data.rbegin(); iter != data.rend(); ++iter)
        {
            if (iter->firstAppearanceTimeUsec == 0) //< Skip if marked.
                continue;

            m_objectIdToTimestampUs[iter->objectId] = iter->firstAppearanceTimeUsec;
            m_data.push_front(std::move(*iter));
        }

        m_latestTimeMs = duration_cast<milliseconds>(
            microseconds(m_data.front().firstAppearanceTimeUsec)).count();
    }

    constrainLength();
}

rest::Handle AnalyticsSearchListModel::Private::getObjects(qint64 startMs, qint64 endMs,
    GetCallback callback, int limit)
{
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return false;

    Filter request;
    request.deviceIds.push_back(camera()->getId());
    request.timePeriod = QnTimePeriod::fromInterval(startMs, endMs);
    request.sortOrder = Qt::DescendingOrder;
    request.maxObjectsToSelect = kFetchBatchSize;
    request.boundingBox = m_filterRect;
    request.freeText = m_filterText;

    const auto internalCallback =
        [callback, guard = QPointer<Private>(this)]
            (bool success, rest::Handle handle, LookupResult&& data)
        {
            if (guard)
                callback(success, handle, std::move(data));
        };

    return server->restConnection()->lookupDetectedObjects(
        request, false /*isLocal*/, internalCallback, thread());
}

void AnalyticsSearchListModel::Private::processMetadata()
{
    QnMutexLocker locker(&m_metadataMutex);
    auto packets = m_metadataPackets;
    m_metadataPackets.clear();
    locker.unlock();

    if (!q->navigator()->isLive() || !q->relevantTimePeriod().isInfinite() || packets.empty())
        return;

    QVector<DetectedObject> newObjects;
    QHash<QnUuid, int> newObjectIndices;

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
        }
    }

    if (newObjects.empty())
        return;

    {
        ScopedInsertRows insertRows(q, 0, int(newObjects.size()) - 1);

        for (const auto& newObject: newObjects)
            m_objectIdToTimestampUs[newObject.objectId] = newObject.firstAppearanceTimeUsec;

        m_data.insert(m_data.begin(),
            std::make_move_iterator(newObjects.begin()),
            std::make_move_iterator(newObjects.end()));
    }

    constrainLength();
}

void AnalyticsSearchListModel::Private::constrainLength()
{
    if (m_data.size() <= kMaximumItemCount)
        return;

    ScopedRemoveRows removeRows(q, kMaximumItemCount, int(m_data.size()) - 1);

    for (auto iter = m_data.cbegin() + kMaximumItemCount; iter != m_data.cend(); ++iter)
        m_objectIdToTimestampUs.remove(iter->objectId);

    m_data.resize(kMaximumItemCount);
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
    auto timeUs = m_objectIdToTimestampUs.value(objectId, -1);
    if (timeUs < 0)
        return -1;

    const auto range = std::make_pair(
        std::lower_bound(m_data.cbegin(), m_data.cend(), timeUs, lowerBoundPredicateUs),
        std::upper_bound(m_data.cbegin(), m_data.cend(), timeUs, upperBoundPredicateUs));

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
    const auto start = timeWatcher->displayTime(startTimeMs(object));
    // TODO: #vkutin Is this duration formula good enough for us?
    //   Or we need to add some "lastAppearanceDurationUsec"?
    const auto durationUs = object.lastAppearanceTimeUsec - object.firstAppearanceTimeUsec;

    using namespace std::chrono;
    return lit("Timestamp: %1 us<br>%2<br>Duration: %3 ms<br>%4")
        .arg(object.firstAppearanceTimeUsec)
        .arg(start.toString(Qt::RFC2822Date))
        .arg(duration_cast<milliseconds>(microseconds(durationUs)).count())
        .arg(object.objectId.toSimpleString());
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
    if (!camera())
        return QSharedPointer<QMenu>();

    // TODO: #vkutin Is this a correct way of choosing servers for analytics actions?
    auto servers = q->cameraHistoryPool()->getCameraFootageData(camera(), true);
    servers.push_back(camera()->getParentServer());

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
    const nx::vms::api::analytics::PluginManifest::ObjectAction& action,
    const analytics::storage::DetectedObject& object) const
{
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return;

    const auto resultCallback =
        [this, guard = QPointer<const Private>(this)]
            (bool success, rest::Handle /*requestId*/, QnJsonRestResult result)
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

qint64 AnalyticsSearchListModel::Private::startTimeMs(
    const DetectedObject& object)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(object.firstAppearanceTimeUsec)).count();
}

AnalyticsSearchListModel::Private::PreviewParams AnalyticsSearchListModel::Private::previewParams(
    const analytics::storage::DetectedObject& object)
{
    PreviewParams result;
    result.timestampUs = object.firstAppearanceTimeUsec;
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

            NX_WARNING(typeid(Private)) << lm("Invalid %1 value: [%2]").args(name, valueStr);
            return defaultValue;
        };

    const QString previewTimestampStr = attribute(lit("nx.sys.preview.timestampUs"));
    if (!previewTimestampStr.isNull())
    {
        const qint64 previewTimestampUs = previewTimestampStr.toLongLong();
        if (previewTimestampUs > 0)
        {
            result.timestampUs = previewTimestampUs;
        }
        else
        {
            NX_WARNING(typeid(Private)) << lm("Invalid nx.sys.preview.timestampUs value: [%1]")
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

} // namespace desktop
} // namespace client
} // namespace nx
