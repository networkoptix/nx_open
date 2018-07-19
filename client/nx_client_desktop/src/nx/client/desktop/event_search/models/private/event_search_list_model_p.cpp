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
#include <nx/utils/raii_guard.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

using nx::vms::api::EventType;
using nx::vms::api::ActionType;
using nx::vms::event::ActionData;
using nx::vms::event::ActionDataList;

namespace {

using namespace std::chrono;

static milliseconds startTime(const ActionData& event)
{
    return duration_cast<milliseconds>(microseconds(event.eventParams.eventTimestampUsec));
}

static const auto lowerBoundPredicate =
    [](const ActionData& left, milliseconds right)
    {
        return startTime(left) > right;
    };

static const auto upperBoundPredicate =
    [](milliseconds left, const ActionData& right)
    {
        return left > startTime(right);
    };

} // namespace

EventSearchListModel::Private::Private(EventSearchListModel* q):
    base_type(q),
    q(q),
    m_helper(new vms::event::StringsHelper(q->commonModule()))
{
}

EventSearchListModel::Private::~Private()
{
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
}

void EventSearchListModel::Private::truncateToMaximumCount()
{
    this->truncateDataToCount(m_data, q->maximumCount());
    if (m_data.empty())
        return;

    auto timeWindow = q->fetchedTimeWindow();
    if (q->fetchDirection() == FetchDirection::earlier)
        timeWindow.truncate(startTime(m_data.front()).count());
    else
        timeWindow.truncateFront(startTime(m_data.back()).count());

    q->setFetchedTimeWindow(timeWindow);
}

void EventSearchListModel::Private::truncateToRelevantTimePeriod()
{
    this->truncateDataToTimePeriod(m_data, upperBoundPredicate, q->relevantTimePeriod());
}

bool EventSearchListModel::Private::hasAccessRights() const
{
    return q->accessController()->hasGlobalPermission(GlobalPermission::viewLogs);
}

rest::Handle EventSearchListModel::Private::requestPrefetch(const QnTimePeriod& period)
{
    const auto eventsReceived =
        [this, period](bool success, rest::Handle requestId, ActionDataList&& data)
        {
            if (!requestId || requestId != currentRequest().id)
                return;

            m_prefetch = success ? std::move(data) : ActionDataList();

            const auto actuallyFetched = m_prefetch.empty()
                ? QnTimePeriod()
                : QnTimePeriod::fromInterval(
                    startTime(m_prefetch.back()), startTime(m_prefetch.front()));

            if (actuallyFetched.isNull())
            {
                NX_VERBOSE(this) << "Pre-fetched no events";
            }
            else
            {
                NX_VERBOSE(this) << "Pre-fetched" << m_prefetch.size() << "events from"
                    << utils::timestampToDebugString(actuallyFetched.startTimeMs) << "to"
                    << utils::timestampToDebugString(actuallyFetched.endTimeMs());
            }

            const bool fetchedAll = success && m_prefetch.size() < currentRequest().batchSize;
            completePrefetch(actuallyFetched, fetchedAll);
        };

    NX_VERBOSE(this) << "Requesting events from"
        << utils::timestampToDebugString(period.startTimeMs) << "to"
        << utils::timestampToDebugString(period.endTimeMs());

    return getEvents(period, eventsReceived, currentRequest().batchSize);
}

template<typename Iter>
bool EventSearchListModel::Private::commitPrefetch(
    const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd, int position)
{
    QnRaiiGuard clearPrefetch([this]() { m_prefetch.clear(); });

    const auto begin = std::lower_bound(prefetchBegin, prefetchEnd,
        periodToCommit.endTime(), lowerBoundPredicate);

    auto end = std::upper_bound(prefetchBegin, prefetchEnd,
        periodToCommit.startTime(), upperBoundPredicate);

    const auto startTimeUs =
        [](const ActionData& event) { return event.eventParams.eventTimestampUsec; };

    // In live mode "later" direction events are requested with 1 ms overlap. Handle overlap here.
    if (q->live() && !m_data.empty() && q->fetchDirection() == FetchDirection::later)
    {
        const auto last = m_data.front();
        const auto lastTimeUs = startTimeUs(last);

        while (end != begin)
        {
            const auto iter = end - 1;
            const auto timeUs = startTimeUs(*iter);

            if (timeUs > lastTimeUs)
                break;

            end = iter;

            if (timeUs == lastTimeUs && iter->businessRuleId == last.businessRuleId)
                break;
        }
    }

    const auto count = std::distance(begin, end);
    if (count <= 0)
    {
        NX_VERBOSE(q) << "Committing no events";
        return false;
    }

    NX_VERBOSE(q) << "Committing" << count << "events from"
        << utils::timestampToDebugString(startTime(*(end - 1)).count()) << "to"
        << utils::timestampToDebugString(startTime(*begin).count());

    ScopedInsertRows insertRows(q, position, position + count - 1);
    m_data.insert(m_data.begin() + position,
        std::make_move_iterator(begin), std::make_move_iterator(end));

    return true;
}

bool EventSearchListModel::Private::commitPrefetch(const QnTimePeriod& periodToCommit)
{
    if (q->fetchDirection() == FetchDirection::earlier)
        return commitPrefetch(periodToCommit, m_prefetch.cbegin(), m_prefetch.cend(), count());

    NX_ASSERT(q->fetchDirection() == FetchDirection::later);
    return commitPrefetch(periodToCommit, m_prefetch.crbegin(), m_prefetch.crend(), 0);
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
    request.limit = limit;
    request.order = q->fetchDirection() == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    const auto internalCallback =
        [callback, guard = QPointer<Private>(this)](
            bool success, rest::Handle handle, rest::EventLogData data)
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
