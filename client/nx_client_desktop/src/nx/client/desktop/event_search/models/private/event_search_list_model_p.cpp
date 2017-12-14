#include "event_search_list_model_p.h"

#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <api/server_rest_connection.h>
#include <api/helpers/event_log_multiserver_request_data.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/synctime.h>

#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kFetchBatchSize = 25;

// In "live mode", every kUpdateTimerInterval newly happened events are fetched.
static constexpr auto kUpdateTimerInterval = std::chrono::seconds(15);

static const auto upperBoundPredicate =
    [](qint64 left, const vms::event::ActionData& right)
    {
        return left > right.eventParams.eventTimestampUsec;
    };

static const auto lowerBoundPredicate =
    [](const vms::event::ActionData& left, qint64 right)
    {
        return left.eventParams.eventTimestampUsec > right;
    };

static qint64 timestampUs(const vms::event::ActionData& event)
{
    return event.eventParams.eventTimestampUsec;
}

static qint64 timestampMs(const vms::event::ActionData& event)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(timestampUs(event))).count();
}

} // namespace

EventSearchListModel::Private::Private(EventSearchListModel* q):
    base_type(),
    q(q),
    m_updateTimer(new QTimer()),
    m_helper(new vms::event::StringsHelper(q->commonModule()))
{
    m_updateTimer->setInterval(std::chrono::milliseconds(kUpdateTimerInterval).count());
    connect(m_updateTimer.data(), &QTimer::timeout, this, &Private::periodicUpdate);
}

EventSearchListModel::Private::~Private()
{
}

QnVirtualCameraResourcePtr EventSearchListModel::Private::camera() const
{
    return m_camera;
}

void EventSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    clear();
    m_camera = camera;
}

vms::event::EventType EventSearchListModel::Private::selectedEventType() const
{
    return m_selectedEventType;
}

void EventSearchListModel::Private::setSelectedEventType(vms::event::EventType value)
{
    if (m_selectedEventType == value)
        return;

    m_selectedEventType = value;
    clear();
}

int EventSearchListModel::Private::count() const
{
    return int(m_data.size());
}

const vms::event::ActionData& EventSearchListModel::Private::getEvent(int index) const
{
    return m_data[index];
}

void EventSearchListModel::Private::clear()
{
    qDebug() << "Clear events model";

    m_updateTimer->stop();

    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_prefetchCompletionHandler = PrefetchCompletionHandler();
    m_fetchedAll = false;
    m_earliestTimeMs = m_latestTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    m_currentFetchId = rest::Handle();
    m_currentUpdateId = rest::Handle();

    if (m_camera)
        m_updateTimer->start();
}

bool EventSearchListModel::Private::canFetchMore() const
{
    return !m_fetchedAll && m_camera && !fetchInProgress()
        && q->accessController()->hasGlobalPermission(Qn::GlobalViewLogsPermission);
}

bool EventSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    const auto eventsReceived =
        [this](bool success, rest::Handle handle, vms::event::ActionDataList&& data)
        {
            if (handle != m_currentFetchId)
                return;

            NX_ASSERT(m_prefetch.empty() && m_prefetchCompletionHandler);
            if (!m_prefetchCompletionHandler)
                return;

            m_prefetch = success ? std::move(data) : vms::event::ActionDataList();
            m_success = success;

            if (m_prefetch.empty())
            {
                qDebug() << "Pre-fetched no events";
            }
            else
            {
                qDebug() << "Pre-fetched" << m_prefetch.size() << "events from"
                    << debugTimestampToString(timestampMs(m_prefetch.back())) << "to"
                    << debugTimestampToString(timestampMs(m_prefetch.front()));
            }

            m_prefetchCompletionHandler(m_prefetch.size() >= kFetchBatchSize
                ? timestampMs(m_prefetch.back()) + 1 /*discard last ms*/
                : 0);

            m_prefetchCompletionHandler = PrefetchCompletionHandler();
        };

    qDebug() << "Requesting events from 0 to" << debugTimestampToString(m_earliestTimeMs - 1);

    m_currentFetchId = getEvents(0, m_earliestTimeMs - 1, eventsReceived, kFetchBatchSize);
    if (!m_currentFetchId)
        return false;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void EventSearchListModel::Private::commitPrefetch(qint64 latestStartTimeMs)
{
    if (!m_currentFetchId)
        return;

    m_currentFetchId = rest::Handle();

    if (!m_success)
    {
        qDebug() << "Committing no events";
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
        qDebug() << "Committing" << count << "events from"
            << debugTimestampToString(timestampMs(m_prefetch[count-1])) << "to"
            << debugTimestampToString(timestampMs(m_prefetch.front()));

        ScopedInsertRows insertRows(q,  first, first + count - 1);
        m_data.insert(m_data.end(),
            std::make_move_iterator(m_prefetch.begin()),
            std::make_move_iterator(end));
    }
    else
    {
        qDebug() << "Committing no events";
    }

    m_fetchedAll = count == m_prefetch.size() && m_prefetch.size() < kFetchBatchSize;
    m_earliestTimeMs = latestStartTimeMs;
    m_prefetch.clear();
}

bool EventSearchListModel::Private::fetchInProgress() const
{
    return m_currentFetchId != rest::Handle();
}

void EventSearchListModel::Private::periodicUpdate()
{
    if (!m_currentUpdateId)
        return;

    const auto eventsReceived =
        [this](bool success, rest::Handle handle, vms::event::ActionDataList&& data)
        {
            if (handle != m_currentUpdateId)
                return;

            m_currentUpdateId = rest::Handle();

            if (success && !data.empty())
                addNewlyReceivedEvents(std::move(data));
        };

    qDebug() << "Requesting new events from" << debugTimestampToString(m_latestTimeMs)
        << "to infinity";

    m_currentUpdateId = getEvents(m_latestTimeMs,
        std::numeric_limits<qint64>::max(), eventsReceived);
}

void EventSearchListModel::Private::addNewlyReceivedEvents(vms::event::ActionDataList&& data)
{
    const auto latestTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(m_latestTimeMs)).count();

    const auto overlapBegin = std::lower_bound(m_data.cbegin(), m_data.cend(),
        latestTimeUs, lowerBoundPredicate);

    const auto alreadyExists =
        [this, &overlapBegin, latestTimeUs](const vms::event::ActionData& event) -> bool
        {
            const auto same =
                [&event](const vms::event::ActionData& other)
                {
                    return event.actionType == other.actionType
                        && event.businessRuleId == other.businessRuleId;
                };

            return std::find_if(overlapBegin, m_data.cend(), same) != m_data.cend();
        };

    // Filter out already added events with exactly latestTimeUs time.
    int count = int(data.size());
    for (auto& event: data)
    {
        if (event.eventParams.eventTimestampUsec > latestTimeUs)
            break;

        if (!alreadyExists(event) && event.actionType != vms::event::undefinedAction)
            continue;

        event.actionType = vms::event::undefinedAction; //< Use as flag.
        --count;
    }

    if (count == 0)
        return;

    ScopedInsertRows insertRows(q,  0, count - 1);
    for (auto iter = data.rbegin(); iter != data.rend(); ++iter)
    {
        if (iter->actionType != vms::event::undefinedAction)
            m_data.push_front(std::move(*iter));
    }

    m_latestTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(m_data.front().eventParams.eventTimestampUsec)).count();
}

rest::Handle EventSearchListModel::Private::getEvents(qint64 startMs, qint64 endMs,
    GetCallback callback, int limit)
{
    if (!m_camera || !callback)
        return false;

    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return false;

    QnEventLogMultiserverRequestData request;
    request.filter.cameras.push_back(m_camera);
    request.filter.period = QnTimePeriod::fromInterval(startMs, endMs);
    request.filter.eventType = m_selectedEventType;
    request.order = Qt::DescendingOrder;
    request.limit = limit;

    const auto internalCallback =
        [callback, guard = QPointer<Private>(this)]
            (bool success, rest::Handle handle, rest::EventLogData data)
        {
            if (guard)
                callback(success, handle, std::move(data.data));
        };

    return server->restConnection()->getEvents(request, internalCallback, thread());
}

QString EventSearchListModel::Private::title(vms::event::EventType eventType) const
{
    return m_helper->eventName(eventType);
}

QString EventSearchListModel::Private::description(
    const vms::event::EventParameters& parameters) const
{
    return m_helper->eventDetails(parameters).join(lit("<br>"));
}

QPixmap EventSearchListModel::Private::pixmap(const vms::event::EventParameters& parameters)
{
    switch (parameters.eventType)
    {
        case nx::vms::event::EventType::storageFailureEvent:
            return qnSkin->pixmap(lit("events/storage_red.png"));

        case nx::vms::event::EventType::backupFinishedEvent:
            return qnSkin->pixmap(lit("events/storage_green.png"));

        case nx::vms::event::EventType::serverStartEvent:
            return qnSkin->pixmap(lit("events/server.png"));

        case nx::vms::event::EventType::serverFailureEvent:
            return qnSkin->pixmap(lit("events/server_red.png"));

        case nx::vms::event::EventType::serverConflictEvent:
            return qnSkin->pixmap(lit("events/server_yellow.png"));

        case nx::vms::event::EventType::licenseIssueEvent:
            return qnSkin->pixmap(lit("events/license_red.png"));

        case nx::vms::event::EventType::cameraDisconnectEvent:
            return qnSkin->pixmap(lit("events/connection_red.png"));

        case nx::vms::event::EventType::networkIssueEvent:
        case nx::vms::event::EventType::cameraIpConflictEvent:
            return qnSkin->pixmap(lit("events/connection_yellow.png"));

        case nx::vms::event::EventType::softwareTriggerEvent:
            return QnSoftwareTriggerPixmaps::colorizedPixmap(
                parameters.description, QPalette().light().color());

        // TODO: #vkutin Fill with actual pixmaps as soon as they're created.
        case nx::vms::event::EventType::cameraMotionEvent:
        case nx::vms::event::EventType::cameraInputEvent:
            return qnSkin->pixmap(lit("tree/camera.png"));

        case nx::vms::event::EventType::analyticsSdkEvent:
            return QPixmap();

        default:
            return QPixmap();
    }
}

QColor EventSearchListModel::Private::color(vms::event::EventType eventType)
{
    switch (eventType)
    {
        case nx::vms::event::EventType::cameraDisconnectEvent:
        case nx::vms::event::EventType::storageFailureEvent:
        case nx::vms::event::EventType::serverFailureEvent:
            return qnGlobals->errorTextColor();

        case nx::vms::event::EventType::networkIssueEvent:
        case nx::vms::event::EventType::cameraIpConflictEvent:
        case nx::vms::event::EventType::serverConflictEvent:
            return qnGlobals->warningTextColor();

        case nx::vms::event::EventType::backupFinishedEvent:
            return qnGlobals->successTextColor();

        //case nx::vms::event::EventType::licenseIssueEvent: //< TODO: normal or warning?
        default:
            return QColor();
    }
}

bool EventSearchListModel::Private::hasPreview(vms::event::EventType eventType)
{
    switch (eventType)
    {
        case vms::event::cameraMotionEvent:
        case vms::event::analyticsSdkEvent:
        case vms::event::cameraInputEvent:
        case vms::event::userDefinedEvent:
            return true;

        default:
            return false;
    }
}

} // namespace
} // namespace client
} // namespace nx
