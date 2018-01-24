#include "analytics_search_list_model_p.h"

#include <chrono>

#include <QtGui/QPalette>

#include <analytics/common/object_detection_metadata.h>
#include <api/server_rest_connection.h>
#include <camera/resource_display.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/synctime.h>

#include <nx/client/core/utils/human_readable.h>
#include <nx/utils/datetime.h>
#include <nx/vms/event/analytics_helper.h>

namespace nx {
namespace client {
namespace desktop {

using namespace analytics::storage;

namespace {

static constexpr int kFetchBatchSize = 25;

static constexpr auto kUpdateTimerInterval = std::chrono::seconds(30);
static constexpr auto kDataChangedInterval = std::chrono::milliseconds(250);

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
        using namespace std::chrono;
        return leftMs > duration_cast<milliseconds>(
            microseconds(right.firstAppearanceTimeUsec)).count();
    };

} // namespace

AnalyticsSearchListModel::Private::Private(AnalyticsSearchListModel* q):
    base_type(q),
    q(q),
    m_updateTimer(new QTimer()),
    m_dataChangedTimer(new QTimer()),
    m_metadataSource(createMetadataSource())
{
    m_updateTimer->setInterval(std::chrono::milliseconds(kUpdateTimerInterval).count());
    connect(m_updateTimer.data(), &QTimer::timeout, this, &Private::periodicUpdate);

    m_dataChangedTimer->setInterval(std::chrono::milliseconds(kDataChangedInterval).count());
    connect(m_dataChangedTimer.data(), &QTimer::timeout, this, &Private::emitDataChangedIfNeeded);
}

AnalyticsSearchListModel::Private::~Private()
{
}

media::SignalingMetadataConsumer* AnalyticsSearchListModel::Private::createMetadataSource()
{
    auto metadataSource = new media::SignalingMetadataConsumer(MetadataType::ObjectDetection);
    connect(metadataSource, &media::SignalingMetadataConsumer::metadataReceived,
        this, &Private::processMetadata);

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
            const auto name = vms::event::AnalyticsHelper::objectName(camera(),
                object.objectTypeId, kDefaultLocale);

            return name.isEmpty() ? tr("Unknown object") : name;
        }

        case Qt::DecorationRole:
            return QVariant::fromValue(qnSkin->pixmap(lit("events/analytics.png")));

        case Qn::DescriptionTextRole:
            return description(object);

        case Qn::TimestampRole:
        case Qn::PreviewTimeRole:
            return startTimeMs(object);

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
            return QVariant::fromValue(object.track.empty()
                ? QRectF()
                : object.track.front().boundingBox);

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
}

void AnalyticsSearchListModel::Private::clear()
{
    qDebug() << "Clear analytics model";

    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_objectIdToTimestampUs.clear();
    m_currentUpdateId = rest::Handle();
    m_latestTimeMs = qMin(qnSyncTime->currentMSecsSinceEpoch(), selectedTimePeriod().endTimeMs());
    m_filterRect = QRectF();
    base_type::clear();

    refreshUpdateTimer();
}

bool AnalyticsSearchListModel::Private::hasAccessRights() const
{
    return q->accessController()->hasGlobalPermission(Qn::GlobalViewLogsPermission);
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
    m_latestTimeMs = qMin(m_latestTimeMs, selectedTimePeriod().endTimeMs());
    refreshUpdateTimer();

    const auto cleanupFunction =
        [this](const DetectedObject& object) { m_objectIdToTimestampUs.remove(object.objectId); };

    // Explicit specialization is required for gcc 4.6.
    clipToTimePeriod<decltype(m_data), decltype(upperBoundPredicateMs)>(
        m_data, upperBoundPredicateMs, selectedTimePeriod(), cleanupFunction);
}

void AnalyticsSearchListModel::Private::refreshUpdateTimer()
{
    if (camera() && selectedTimePeriod().isInfinite())
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

    ScopedInsertRows insertRows(q, 0, added - 1);
    for (auto iter = data.rbegin(); iter != data.rend(); ++iter)
    {
        if (iter->firstAppearanceTimeUsec != 0) //< If not marked to skip...
            m_data.push_front(std::move(*iter));
    }

    m_latestTimeMs = duration_cast<milliseconds>(
        microseconds(m_data.front().firstAppearanceTimeUsec)).count();
}

rest::Handle AnalyticsSearchListModel::Private::getObjects(qint64 startMs, qint64 endMs,
    GetCallback callback, int limit)
{
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return false;

    Filter request;
    request.deviceId = camera()->getId();
    request.timePeriod = QnTimePeriod::fromInterval(startMs, endMs);
    request.sortOrder = Qt::DescendingOrder;
    request.maxObjectsToSelect = kFetchBatchSize;
    request.boundingBox = m_filterRect;

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

void AnalyticsSearchListModel::Private::processMetadata(
    const QnAbstractCompressedMetadataPtr& metadata)
{
    NX_EXPECT(metadata->metadataType == MetadataType::ObjectDetection);
    const auto compressedMetadata = std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
    const auto detectionMetadata = common::metadata::fromMetadataPacket(compressedMetadata);

    if (!detectionMetadata || detectionMetadata->objects.empty() || !q->navigator()->isLive())
        return;

    std::vector<DetectedObject> newObjects;
    newObjects.reserve(detectionMetadata->objects.size());

    ObjectPosition pos;
    pos.deviceId = detectionMetadata->deviceId;
    pos.timestampUsec = detectionMetadata->timestampUsec;
    pos.durationUsec = detectionMetadata->durationUsec;

    for (const auto& item: detectionMetadata->objects)
    {
        pos.boundingBox = item.boundingBox;

        const auto index = indexOf(item.objectId);
        if (index < 0)
        {
            if (m_filterRect.isValid() && !m_filterRect.intersects(item.boundingBox))
                continue;

            DetectedObject newObject;
            newObject.objectId = item.objectId;
            newObject.objectTypeId = item.objectTypeId;
            newObject.attributes = std::move(item.labels);
            newObject.track.push_back(pos);
            newObject.firstAppearanceTimeUsec = pos.timestampUsec;
            newObject.lastAppearanceTimeUsec = pos.timestampUsec;
            newObjects.push_back(std::move(newObject));
        }
        else
        {
            pos.attributes = std::move(item.labels);
            advanceObject(m_data[index], std::move(pos));
        }
    }

    if (newObjects.empty())
        return;

    ScopedInsertRows insertRows(q, 0, int(newObjects.size()) - 1);

    for (const auto& newObject: newObjects)
        m_objectIdToTimestampUs[newObject.objectId] = newObject.firstAppearanceTimeUsec;

    m_data.insert(m_data.begin(),
        std::make_move_iterator(newObjects.begin()),
        std::make_move_iterator(newObjects.end()));
}

void AnalyticsSearchListModel::Private::emitDataChangedIfNeeded()
{
    if (m_dataChangedObjectIds.empty())
        return;

    static const QVector<int> kUpdateRoles({Qt::ToolTipRole, Qn::DescriptionTextRole});
    for (const auto& id: m_dataChangedObjectIds)
    {
        const auto index = indexOf(id);
        if (index < 0)
            continue;

        const auto modelIndex = q->index(index);
        emit q->dataChanged(modelIndex, modelIndex, kUpdateRoles);
    }

    m_dataChangedObjectIds.clear();
};

void AnalyticsSearchListModel::Private::advanceObject(DetectedObject& object,
    ObjectPosition&& position)
{
    // Remove object-related attributes from position.
    for (int i = int(position.attributes.size()) - 1; i >= 0; --i)
    {
        if (std::find(object.attributes.cbegin(), object.attributes.cend(), position.attributes[i])
            != object.attributes.cend())
        {
            position.attributes.erase(position.attributes.begin() + i);
        }
    }

    object.lastAppearanceTimeUsec = position.timestampUsec;
    m_dataChangedObjectIds.insert(object.objectId);
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

bool AnalyticsSearchListModel::Private::defaultAction(int index) const
{
    // TODO: #vkutin Introduce a new QnAction instead of direct access.
    if (auto slider = q->navigator()->timeSlider())
    {
        const QnScopedTypedPropertyRollback<bool, QnTimeSlider> downRollback(slider,
            &QnTimeSlider::setSliderDown,
            &QnTimeSlider::isSliderDown,
            true);

        slider->setValue(startTimeMs(m_data[index]), true);
        return true;
    }

    return false;
}

QString AnalyticsSearchListModel::Private::description(
    const DetectedObject& object) const
{
    QString result;

    const auto timeWatcher = q->context()->instance<QnWorkbenchServerTimeWatcher>();
    const auto start = timeWatcher->displayTime(startTimeMs(object));
    // TODO: #vkutin Is this duration formula good enough for us?
    //   Or we need to add some "lastAppearanceDurationUsec"?
    const auto durationUs = object.lastAppearanceTimeUsec - object.firstAppearanceTimeUsec;

    result = lit("%1: %2<br>%3: %4")
        .arg(tr("Start"))
        .arg(start.toString(Qt::RFC2822Date))
        .arg(tr("Duration"))
        .arg(core::HumanReadable::timeSpan(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::microseconds(durationUs))));

    if (object.attributes.empty())
        return result;

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
        rows += kRowTemplate.arg(attribute.name, attribute.value);

    const auto color = QPalette().color(QPalette::WindowText);
    result += lit("<br>") + kCss.arg(color.name()) + kTableTemplate.arg(rows);

    return result;
}

qint64 AnalyticsSearchListModel::Private::startTimeMs(
    const DetectedObject& object)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(object.firstAppearanceTimeUsec)).count();
}

} // namespace desktop
} // namespace client
} // namespace nx
