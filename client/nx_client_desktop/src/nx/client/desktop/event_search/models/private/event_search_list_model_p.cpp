#include "event_search_list_model_p.h"

#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <api/server_rest_connection.h>
#include <api/helpers/event_log_multiserver_request_data.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/synctime.h>

#include <nx/utils/datetime.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace {

using namespace std::chrono;

// In "live mode", every kUpdateTimerInterval newly happened events are fetched.
static constexpr milliseconds kUpdateTimerInterval = std::chrono::seconds(15);

static microseconds timestamp(const vms::event::ActionData& event)
{
    return microseconds(event.eventParams.eventTimestampUsec);
}

static const auto lowerBoundPredicate =
    [](const vms::event::ActionData& left, milliseconds right)
    {
        return timestamp(left) > right;
    };

static const auto upperBoundPredicate =
    [](milliseconds left, const vms::event::ActionData& right)
    {
        return left > timestamp(right);
    };

} // namespace

EventSearchListModel::Private::Private(EventSearchListModel* q):
    base_type(q),
    q(q),
    m_updateTimer(new QTimer()),
    m_helper(new vms::event::StringsHelper(q->commonModule()))
{
    m_updateTimer->setInterval(kUpdateTimerInterval.count());
    connect(m_updateTimer.data(), &QTimer::timeout, this, &Private::periodicUpdate);
}

EventSearchListModel::Private::~Private()
{
}

void EventSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (camera == this->camera())
        return;

    base_type::setCamera(camera);
    refreshUpdateTimer();
}

vms::api::EventType EventSearchListModel::Private::selectedEventType() const
{
    return m_selectedEventType;
}

void EventSearchListModel::Private::setSelectedEventType(vms::api::EventType value)
{
    if (m_selectedEventType == value)
        return;

    m_selectedEventType = value;
    q->clear();
}

int EventSearchListModel::Private::count() const
{
    return int(m_data.size());
}

QVariant EventSearchListModel::Private::data(const QModelIndex& index, int role,
    bool& handled) const
{
    const auto& event = m_data[index.row()];
    handled = true;

    switch (role)
    {
        case Qt::DisplayRole:
            return title(event.eventParams.eventType);

        case Qt::DecorationRole:
            return QVariant::fromValue(pixmap(event.eventParams));

        case Qt::ForegroundRole:
            return QVariant::fromValue(color(event.eventParams.eventType));

        case Qn::DescriptionTextRole:
            return description(event.eventParams);

        case Qn::PreviewTimeRole:
            if (!hasPreview(event.eventParams.eventType))
                return QVariant();
            /*fallthrough*/
        case Qn::TimestampRole:
            return QVariant::fromValue(event.eventParams.eventTimestampUsec);

        case Qn::ResourceRole:
            return hasPreview(event.eventParams.eventType)
                ? QVariant::fromValue<QnResourcePtr>(camera())
                : QVariant();

        case Qn::HelpTopicIdRole:
            return Qn::Empty_Help;

        default:
            handled = false;
            return QVariant();
    }
}

void EventSearchListModel::Private::clearData()
{
    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_currentUpdateId = rest::Handle();
    m_latestTime = qMin(std::chrono::milliseconds(qnSyncTime->currentMSecsSinceEpoch()),
        q->relevantTimePeriod().endTime());
    refreshUpdateTimer();
}

void EventSearchListModel::Private::truncateToMaximumCount()
{
    this->truncateDataToCount(m_data, q->maximumCount());
    if (m_data.empty())
        return;

    auto timeWindow = q->fetchedTimeWindow();
    if (q->fetchDirection() == FetchDirection::earlier)
    {
        timeWindow.setEndTime(qMin(timeWindow.endTime(),
            duration_cast<milliseconds>(timestamp(m_data.front()))));
    }
    else
    {
        timeWindow.setStartTime(qMax(timeWindow.startTime(),
            duration_cast<milliseconds>(timestamp(m_data.back()))));
    }

    q->setFetchedTimeWindow(timeWindow);
}

void EventSearchListModel::Private::truncateToRelevantTimePeriod()
{
    // TODO: FIXME!!!
    m_currentUpdateId = rest::Handle(); //< Cancel timed update.
    m_latestTime = qMin(m_latestTime, q->relevantTimePeriod().endTime());
    refreshUpdateTimer();
    this->truncateDataToTimePeriod(m_data, upperBoundPredicate, q->relevantTimePeriod());
}

bool EventSearchListModel::Private::hasAccessRights() const
{
    return q->accessController()->hasGlobalPermission(GlobalPermission::viewLogs);
}

rest::Handle EventSearchListModel::Private::requestPrefetch(const QnTimePeriod& period)
{
    const auto eventsReceived =
        [this, period](
            bool success,
            rest::Handle requestId,
            vms::event::ActionDataList&& data)
        {
            if (!requestId || requestId != currentRequest().id)
                return;

            m_prefetch = success ? std::move(data) : vms::event::ActionDataList();
            m_success = success;

            const auto actuallyFetched = m_prefetch.empty()
                ? QnTimePeriod()
                : QnTimePeriod::fromInterval(
                    duration_cast<milliseconds>(timestamp(m_prefetch.back())),
                    duration_cast<milliseconds>(timestamp(m_prefetch.front())));

            if (actuallyFetched.isNull())
            {
                NX_VERBOSE(this) << "Pre-fetched no events";
            }
            else
            {
                NX_VERBOSE(this) << "Pre-fetched" << m_prefetch.size() << "events from"
                    << utils::timestampToRfc2822(actuallyFetched.startTimeMs) << "to"
                    << utils::timestampToRfc2822(actuallyFetched.endTimeMs());
            }

            completePrefetch(actuallyFetched, int(m_prefetch.size()));
        };

    NX_VERBOSE(this) << "Requesting events from"
        << utils::timestampToRfc2822(period.startTimeMs) << "to"
        << utils::timestampToRfc2822(period.endTimeMs());

    return getEvents(period, eventsReceived, currentRequest().batchSize);
}

bool EventSearchListModel::Private::commitPrefetch(const QnTimePeriod& periodToCommit)
{
    if (!m_success)
    {
        NX_VERBOSE(this) << "Committing no events";
        return false;
    }

    const auto begin = std::lower_bound(m_prefetch.cbegin(), m_prefetch.cend(),
        periodToCommit.endTime(), lowerBoundPredicate);

    const auto end = std::upper_bound(m_prefetch.cbegin(), m_prefetch.cend(),
        periodToCommit.startTime(), upperBoundPredicate);

    const auto timestampMs =
        [](const vms::event::ActionData& event)
        {
            return duration_cast<std::chrono::milliseconds>(timestamp(event)).count();
        };

    const auto count = std::distance(begin, end);
    if (count > 0)
    {
        NX_VERBOSE(this) << "Committing" << count << "events from"
            << utils::timestampToRfc2822(timestampMs(*(end - 1))) << "to"
            << utils::timestampToRfc2822(timestampMs(*begin));

        if (q->fetchDirection() == FetchDirection::earlier)
        {
            const auto first = this->count();
            ScopedInsertRows insertRows(q, first, first + count - 1);
            m_data.insert(m_data.end(), std::make_move_iterator(begin), std::make_move_iterator(end));
        }
        else
        {
            NX_ASSERT(q->fetchDirection() == FetchDirection::later);
            ScopedInsertRows insertRows(q, 0, count - 1);
            m_data.insert(m_data.begin(), std::make_move_iterator(begin), std::make_move_iterator(end));
        }
    }
    else
    {
        NX_VERBOSE(this) << "Committing no events";
    }

    m_prefetch.clear();
    return true;
}

void EventSearchListModel::Private::refreshUpdateTimer()
{
    if (camera() && q->relevantTimePeriod().isInfinite())
    {
        if (!m_updateTimer->isActive())
        {
            NX_VERBOSE(this) << "Event search update timer started";
            m_updateTimer->start();
        }
    }
    else
    {
        if (m_updateTimer->isActive())
        {
            NX_VERBOSE(this) << "Event search update timer stopped";
            m_updateTimer->stop();
        }
    }
}

void EventSearchListModel::Private::periodicUpdate()
{
    if (m_currentUpdateId)
        return;

    const auto eventsReceived =
        [this](bool success, rest::Handle handle, vms::event::ActionDataList&& data)
        {
            if (handle != m_currentUpdateId)
                return;

            m_currentUpdateId = rest::Handle();

            if (success && !data.empty())
                addNewlyReceivedEvents(std::move(data));
            else
                NX_VERBOSE(this) << "Periodic update: no new events added";
        };

    NX_VERBOSE(this) << "Periodic update: requesting new events from"
        << utils::timestampToRfc2822(m_latestTime.count()) << "to infinity";

    m_currentUpdateId = getEvents(
        QnTimePeriod(m_latestTime.count(), QnTimePeriod::infiniteDuration()),
        eventsReceived);
}

void EventSearchListModel::Private::addNewlyReceivedEvents(vms::event::ActionDataList&& data)
{
    const auto overlapBegin = std::lower_bound(m_data.cbegin(), m_data.cend(),
        m_latestTime, lowerBoundPredicate);

    const auto alreadyExists =
        [this, &overlapBegin](const vms::event::ActionData& event) -> bool
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
        if (timestamp(event) > m_latestTime)
            break;

        if (!alreadyExists(event) && event.actionType != ActionType::undefinedAction)
            continue;

        event.actionType = ActionType::undefinedAction; //< Use as flag.
        --count;
    }

    NX_VERBOSE(this) << "Periodic update:" << count << "new events added";

    if (count == 0)
        return;

    ScopedInsertRows insertRows(q,  0, count - 1);
    for (auto iter = data.rbegin(); iter != data.rend(); ++iter)
    {
        if (iter->actionType != ActionType::undefinedAction)
            m_data.push_front(std::move(*iter));
    }

    using namespace std::chrono;
    m_latestTime = duration_cast<milliseconds>(timestamp(m_data.front()));
}

rest::Handle EventSearchListModel::Private::getEvents(const QnTimePeriod& period,
    GetCallback callback, int limit)
{
    if (!camera() || !callback)
        return false;

    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->restConnection());
    if (!server || !server->restConnection())
        return false;

    QnEventLogMultiserverRequestData request;
    request.filter.cameras.push_back(camera());
    request.filter.period = period;
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

QString EventSearchListModel::Private::title(vms::api::EventType eventType) const
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
        case nx::vms::api::EventType::storageFailureEvent:
            return qnSkin->pixmap(lit("events/storage_red.png"));

        case nx::vms::api::EventType::backupFinishedEvent:
            return qnSkin->pixmap(lit("events/storage_green.png"));

        case nx::vms::api::EventType::serverStartEvent:
            return qnSkin->pixmap(lit("events/server.png"));

        case nx::vms::api::EventType::serverFailureEvent:
            return qnSkin->pixmap(lit("events/server_red.png"));

        case nx::vms::api::EventType::serverConflictEvent:
            return qnSkin->pixmap(lit("events/server_yellow.png"));

        case nx::vms::api::EventType::licenseIssueEvent:
            return qnSkin->pixmap(lit("events/license_red.png"));

        case nx::vms::api::EventType::cameraDisconnectEvent:
            return qnSkin->pixmap(lit("events/connection_red.png"));

        case nx::vms::api::EventType::networkIssueEvent:
        case nx::vms::api::EventType::cameraIpConflictEvent:
            return qnSkin->pixmap(lit("events/connection_yellow.png"));

        case nx::vms::api::EventType::softwareTriggerEvent:
            return QnSoftwareTriggerPixmaps::colorizedPixmap(
                parameters.description, QPalette().light().color());

        // TODO: #vkutin Fill with actual pixmaps as soon as they're created.
        case nx::vms::api::EventType::cameraMotionEvent:
        case nx::vms::api::EventType::cameraInputEvent:
            return qnSkin->pixmap(lit("tree/camera.png"));

        case nx::vms::api::EventType::analyticsSdkEvent:
            return QPixmap();

        default:
            return QPixmap();
    }
}

QColor EventSearchListModel::Private::color(vms::api::EventType eventType)
{
    switch (eventType)
    {
        case nx::vms::api::EventType::cameraDisconnectEvent:
        case nx::vms::api::EventType::storageFailureEvent:
        case nx::vms::api::EventType::serverFailureEvent:
            return qnGlobals->errorTextColor();

        case nx::vms::api::EventType::networkIssueEvent:
        case nx::vms::api::EventType::cameraIpConflictEvent:
        case nx::vms::api::EventType::serverConflictEvent:
            return qnGlobals->warningTextColor();

        case nx::vms::api::EventType::backupFinishedEvent:
            return qnGlobals->successTextColor();

        //case nx::vms::api::EventType::licenseIssueEvent: //< TODO: normal or warning?
        default:
            return QColor();
    }
}

bool EventSearchListModel::Private::hasPreview(vms::api::EventType eventType)
{
    switch (eventType)
    {
        case EventType::cameraMotionEvent:
        case EventType::analyticsSdkEvent:
        case EventType::cameraInputEvent:
        case EventType::userDefinedEvent:
            return true;

        default:
            return false;
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
