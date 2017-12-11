#include "analytics_search_list_model_p.h"

#include <chrono>

#include <QtGui/QPalette>

#include <analytics/common/object_detection_metadata.h>
#include <api/server_rest_connection.h>
#include <camera/resource_display.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/style/helper.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include <nx/client/core/utils/human_readable.h>

namespace nx {
namespace client {
namespace desktop {

using namespace analytics::storage;

namespace {

static constexpr int kFetchBatchSize = 25;

static constexpr auto kUpdateTimerInterval = std::chrono::seconds(30);

static void advanceObject(analytics::storage::DetectedObject& object,
    analytics::storage::ObjectPosition&& position)
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
}

static const auto lowerBoundPredicate =
    [](const DetectedObject& left, qint64 right)
    {
        return left.firstAppearanceTimeUsec > right;
    };

static const auto upperBoundPredicate =
    [](qint64 left, const DetectedObject& right)
    {
        return left > right.firstAppearanceTimeUsec;
    };

} // namespace

AnalyticsSearchListModel::Private::Private(AnalyticsSearchListModel* q):
    base_type(),
    q(q),
    m_updateTimer(new QTimer()),
    m_metadataSource(createMetadataSource())
{
    m_updateTimer->setInterval(std::chrono::milliseconds(kUpdateTimerInterval).count());
    connect(m_updateTimer.data(), &QTimer::timeout, this, &Private::periodicUpdate);
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

QnVirtualCameraResourcePtr AnalyticsSearchListModel::Private::camera() const
{
    return m_camera;
}

void AnalyticsSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    clear();
    m_camera = camera;

    if (m_display)
        m_display->removeMetadataConsumer(m_metadataSource);

    m_display.reset();

    if (!m_camera)
        return;

    auto widget = qobject_cast<QnMediaResourceWidget*>(q->navigator()->currentWidget());
    NX_ASSERT(widget && widget->resource() == m_camera);

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

const analytics::storage::DetectedObject& AnalyticsSearchListModel::Private::object(int index) const
{
    return m_data[index];
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
    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_fetchedAll = false;
    m_earliestTimeMs = m_latestTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    m_currentFetchId = rest::Handle();
}

bool AnalyticsSearchListModel::Private::canFetchMore() const
{
    return !m_fetchedAll && m_camera && !m_currentFetchId
        && q->accessController()->hasGlobalPermission(Qn::GlobalViewLogsPermission);
}

bool AnalyticsSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    const auto dataReceived =
        [this, completionHandler]
            (bool success, rest::Handle handle, analytics::storage::LookupResult&& data)
        {
            if (m_currentFetchId != handle)
                return;

            NX_ASSERT(m_prefetch.empty());
            m_prefetch = success ? std::move(data) : analytics::storage::LookupResult();
            m_success = success;

            if (m_prefetch.empty())
            {
                qDebug() << "Pre-fetched no analytics";
            }
            else
            {
                qDebug() << "Pre-fetched" << m_prefetch.size() << "analytics from"
                    << debugTimestampToString(startTimeMs(m_prefetch.back())) << "to"
                    << debugTimestampToString(startTimeMs(m_prefetch.front()));
            }

            NX_ASSERT(m_prefetch.empty() || !m_prefetch.front().track.empty());
            completionHandler(m_prefetch.size() >= kFetchBatchSize
                ? startTimeMs(m_prefetch.back()) + 1 /*discard last ms*/
                : 0);
        };

    qDebug() << "Requesting analytics from 0 to" << debugTimestampToString(m_earliestTimeMs - 1);

    m_currentFetchId = getObjects(0, m_earliestTimeMs - 1, dataReceived, kFetchBatchSize);
    return m_currentFetchId != rest::Handle();
}

void AnalyticsSearchListModel::Private::commitPrefetch(qint64 latestStartTimeMs)
{
    if (!m_currentFetchId)
        return;

    m_currentFetchId = rest::Handle();

    if (!m_success)
    {
        qDebug() << "Committing no analytics";
        return;
    }

    const auto latestTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(latestStartTimeMs)).count();

    const auto end = std::upper_bound(m_prefetch.begin(), m_prefetch.end(),
        latestTimeUs, upperBoundPredicate);

    const auto first = this->count();
    const auto count = std::distance(m_prefetch.begin(), end);

    if (count > 0)
    {
        qDebug() << "Committing" << count << "analytics from"
            << debugTimestampToString(startTimeMs(m_prefetch[count - 1])) << "to"
            << debugTimestampToString(startTimeMs(m_prefetch.front()));

        ScopedInsertRows insertRows(q,  first, first + count - 1);
        m_data.insert(m_data.end(),
            std::make_move_iterator(m_prefetch.begin()),
            std::make_move_iterator(end));
    }
    else
    {
        qDebug() << "Committing no analytics";
    }

    m_fetchedAll = m_prefetch.empty();
    m_earliestTimeMs = latestStartTimeMs;
    m_prefetch.clear();
}

void AnalyticsSearchListModel::Private::periodicUpdate()
{
    return; // TODO: #vkutin Implement addNewlyReceivedObjects

    if (!m_currentUpdateId)
        return;

    const auto eventsReceived =
        [this](bool success, rest::Handle handle, analytics::storage::LookupResult&& data)
        {
            if (handle != m_currentUpdateId)
                return;

            m_currentUpdateId = rest::Handle();

            if (success && !data.empty())
                addNewlyReceivedObjects(std::move(data));
        };

    qDebug() << "Requesting new analytics from" << debugTimestampToString(m_latestTimeMs)
        << "to infinity";

    m_currentUpdateId = getObjects(m_latestTimeMs,
        std::numeric_limits<qint64>::max(), eventsReceived);
}

void AnalyticsSearchListModel::Private::addNewlyReceivedObjects(
    analytics::storage::LookupResult&& data)
{
    // TODO: #vkutin Implement me!
}

rest::Handle AnalyticsSearchListModel::Private::getObjects(qint64 startMs, qint64 endMs,
    GetCallback callback, int limit)
{
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return false;

    analytics::storage::Filter request;
    request.deviceId = m_camera->getId();
    request.timePeriod = QnTimePeriod::fromInterval(0, m_earliestTimeMs - 1);
    request.sortOrder = Qt::DescendingOrder;
    request.maxObjectsToSelect = kFetchBatchSize;
    request.boundingBox = m_filterRect;

    const auto internalCallback =
        [callback, guard = QPointer<Private>(this)]
            (bool success, rest::Handle handle, analytics::storage::LookupResult&& data)
        {
            if (guard)
                callback(success, handle, std::move(data));
        };

    return server->restConnection()->lookupDetectedObjects(
        request, internalCallback, thread());
}

void AnalyticsSearchListModel::Private::processMetadata(
    const QnAbstractCompressedMetadataPtr& metadata)
{
    NX_EXPECT(metadata->metadataType == MetadataType::ObjectDetection);
    const auto compressedMetadata = std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
    const auto detectionMetadata = common::metadata::fromMetadataPacket(compressedMetadata);

    if (!detectionMetadata || !q->navigator()->isLive())
        return;

    if (detectionMetadata->objects.empty())
    {
        m_lastObjectTimesUs.clear();
        return;
    }

    std::vector<analytics::storage::DetectedObject> newObjects;
    newObjects.reserve(detectionMetadata->objects.size());

    analytics::storage::ObjectPosition pos;
    pos.deviceId = detectionMetadata->deviceId;
    pos.timestampUsec = detectionMetadata->timestampUsec;
    pos.durationUsec = detectionMetadata->durationUsec;

    QSet<QnUuid> finishedObjects;
    for (auto iter = m_lastObjectTimesUs.cbegin(); iter != m_lastObjectTimesUs.cend(); ++iter)
        finishedObjects.insert(iter.key());

    for (const auto& item: detectionMetadata->objects)
    {
        const auto iter = m_lastObjectTimesUs.find(item.objectId);
        pos.boundingBox = item.boundingBox;

        const auto index = iter != m_lastObjectTimesUs.cend()
            ? indexOf(item.objectId, iter.value())
            : -1;

        if (index < 0)
        {
            if (m_filterRect.isValid() && !m_filterRect.intersects(item.boundingBox))
                continue;

            analytics::storage::DetectedObject newObject;
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
            finishedObjects.remove(item.objectId);

            const auto emitDataChanged =
                [this, timeUs = m_data[index].firstAppearanceTimeUsec, id = m_data[index].objectId]()
                {
                    static const QVector<int> kUpdateRoles
                        {Qt::ToolTipRole, Qn::DescriptionTextRole};

                    const auto index = q->index(indexOf(id, timeUs));
                    if (index.isValid())
                        emit q->dataChanged(index, index, kUpdateRoles);
                };

            static constexpr int kDataChangedIntervalMs = 250;
            if (m_dataChangedTimer)
                m_dataChangedTimer->deleteLater();
            m_dataChangedTimer =
                executeDelayedParented(emitDataChanged, kDataChangedIntervalMs, this);
        }
    }

    for (const auto& id: finishedObjects)
        m_lastObjectTimesUs.remove(id);

    if (newObjects.empty())
        return;

    ScopedInsertRows insertRows(q, 0, int(newObjects.size()) - 1);

    for (const auto& newObject: newObjects)
        m_lastObjectTimesUs[newObject.objectId] = detectionMetadata->timestampUsec;

    m_data.insert(m_data.begin(),
        std::make_move_iterator(newObjects.begin()),
        std::make_move_iterator(newObjects.end()));
}

int AnalyticsSearchListModel::Private::indexOf(const QnUuid& objectId, qint64 timeUs) const
{
    const auto range = std::make_pair(
        std::lower_bound(m_data.cbegin(), m_data.cend(), timeUs, lowerBoundPredicate),
        std::upper_bound(m_data.cbegin(), m_data.cend(), timeUs, upperBoundPredicate));

    const auto iter = std::find_if(range.first, range.second,
        [&objectId](const DetectedObject& item) { return item.objectId == objectId; });

    return iter != range.second ? int(std::distance(m_data.cbegin(), iter)) : -1;
}

QString AnalyticsSearchListModel::Private::description(
    const analytics::storage::DetectedObject& object) const
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
    const analytics::storage::DetectedObject& object)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(object.firstAppearanceTimeUsec)).count();
}

} // namespace
} // namespace client
} // namespace nx
