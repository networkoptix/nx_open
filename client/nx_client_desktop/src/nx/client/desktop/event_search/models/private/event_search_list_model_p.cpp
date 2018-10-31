#include "event_search_list_model_p.h"

#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <api/server_rest_connection.h>
#include <api/helpers/event_log_multiserver_request_data.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/help/help_topics.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/synctime.h>

#include <nx/client/desktop/utils/managed_camera_set.h>
#include <nx/utils/datetime.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/event/strings_helper.h>

namespace nx::client::desktop {

using nx::vms::api::EventType;
using nx::vms::api::ActionType;
using nx::vms::event::ActionData;
using nx::vms::event::ActionDataList;

using namespace std::chrono;
using namespace std::literals::chrono_literals;

namespace {

static constexpr auto kLiveUpdateInterval = 15s;

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
    m_helper(new vms::event::StringsHelper(q->commonModule())),
    m_liveUpdateTimer(new QTimer())
{
    connect(m_liveUpdateTimer.data(), &QTimer::timeout, this, &Private::fetchLive);
    m_liveUpdateTimer->start(kLiveUpdateInterval);
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

QString EventSearchListModel::Private::selectedSubType() const
{
    return m_selectedSubType;
}

void EventSearchListModel::Private::setSelectedSubType(const QString& value)
{
    if (m_selectedSubType == value)
        return;

    m_selectedSubType = value;
    q->clear();
}

int EventSearchListModel::Private::count() const
{
    return int(m_data.size());
}

QVariant EventSearchListModel::Private::data(const QModelIndex& index, int role,
    bool& handled) const
{
    const auto& eventParams = m_data[index.row()].eventParams;
    handled = true;

    switch (role)
    {
        case Qt::DisplayRole:
            return title(eventParams.eventType);

        case Qt::DecorationRole:
            return QVariant::fromValue(pixmap(eventParams));

        case Qt::ForegroundRole:
            return QVariant::fromValue(color(eventParams.eventType));

        case Qn::DescriptionTextRole:
            return description(eventParams);

        case Qn::PreviewTimeRole:
            if (!hasPreview(eventParams.eventType))
                return QVariant();
            [[fallthrough]];
        case Qn::TimestampRole:
            return QVariant::fromValue(eventParams.eventTimestampUsec);

        case Qn::ResourceListRole:
        {
            // TODO: #vkutin Display "[camera removed]" or "[server removed]"
            // for event sources no longer in the system.

            if (eventParams.eventResourceId.isNull() && !eventParams.resourceName.isEmpty())
                return QVariant::fromValue(QStringList({eventParams.resourceName}));

            const auto resource = q->resourcePool()->getResourceById(eventParams.eventResourceId);
            return resource
                ? QVariant::fromValue(QnResourceList({resource}))
                : QVariant(); //< TODO: #vkutin Replace with <deleted camera> or <deleted server>?
        }

        case Qn::ResourceRole: //< Resource for thumbnail preview only.
        {
            if (!hasPreview(eventParams.eventType))
                return false;

            return QVariant::fromValue<QnResourcePtr>(q->resourcePool()->
                getResourceById<QnVirtualCameraResource>(eventParams.eventResourceId));
        }

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
    m_liveFetch = {};
}

void EventSearchListModel::Private::truncateToMaximumCount()
{
    this->truncateDataToMaximumCount(m_data, &startTime);
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
        [this](bool success, rest::Handle requestId, ActionDataList&& data)
        {
            if (!requestId || requestId != currentRequest().id)
                return;

            QnTimePeriod actuallyFetched;
            m_prefetch = vms::event::ActionDataList();

            if (success)
            {
                m_prefetch = std::move(data);
                if (!m_prefetch.empty())
                {
                    actuallyFetched = QnTimePeriod::fromInterval(
                        startTime(m_prefetch.back()), startTime(m_prefetch.front()));
                }
            }

            completePrefetch(actuallyFetched, success, int(m_prefetch.size()));
        };

    const auto sortOrder = currentRequest().direction == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    return getEvents(period, eventsReceived, sortOrder, currentRequest().batchSize);
}

template<typename Iter>
bool EventSearchListModel::Private::commitInternal(const QnTimePeriod& periodToCommit,
    Iter prefetchBegin, Iter prefetchEnd, int position, bool handleOverlaps)
{
    const auto begin = std::lower_bound(prefetchBegin, prefetchEnd,
        periodToCommit.endTime(), lowerBoundPredicate);

    auto end = std::upper_bound(prefetchBegin, prefetchEnd,
        periodToCommit.startTime(), upperBoundPredicate);

    if (!m_data.empty() && handleOverlaps)
    {
        const auto& last = m_data.front();
        const auto lastTimeUs = last.eventParams.eventTimestampUsec;

        while (end != begin)
        {
            const auto iter = end - 1;
            const auto timeUs = iter->eventParams.eventTimestampUsec;

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
    const auto clearPrefetch = nx::utils::makeScopeGuard([this]() { m_prefetch.clear(); });

    if (currentRequest().direction == FetchDirection::earlier)
        return commitInternal(periodToCommit, m_prefetch.begin(), m_prefetch.end(), count(), false);

    NX_ASSERT(currentRequest().direction == FetchDirection::later);
    return commitInternal(
        periodToCommit, m_prefetch.rbegin(), m_prefetch.rend(), 0, q->effectiveLiveSupported());
}

void EventSearchListModel::Private::fetchLive()
{
    if (m_liveFetch.id || !q->isLive() || q->livePaused())
        return;

    if (m_data.empty() && fetchInProgress())
        return; //< Don't fetch live if first fetch from archive is in progress.

    const milliseconds from = (m_data.empty() ? 0ms : startTime(m_data.front()));
    m_liveFetch.period = QnTimePeriod(from.count(), QnTimePeriod::infiniteDuration());
    m_liveFetch.direction = FetchDirection::later;
    m_liveFetch.batchSize = q->fetchBatchSize();

    const auto liveEventsReceived =
        [this](bool success, rest::Handle requestId, ActionDataList&& data)
        {
            const auto scopedClear = nx::utils::makeScopeGuard([this]() { m_liveFetch = {}; });

            if (!success || data.empty() || !q->isLive() || !requestId || requestId != m_liveFetch.id)
                return;

            auto periodToCommit = QnTimePeriod::fromInterval(
                startTime(data.back()), startTime(data.front()));

            if (data.size() >= m_liveFetch.batchSize)
            {
                periodToCommit.truncateFront(periodToCommit.startTimeMs + 1);
                q->clear(); //< Otherwise there will be a gap between live and archive events.
            }

            q->addToFetchedTimeWindow(periodToCommit);

            NX_VERBOSE(q) << "Live update commit";
            commitInternal(periodToCommit, data.begin(), data.end(), 0, true);

            if (count() > q->maximumCount())
            {
                NX_VERBOSE(q) << "Truncating to maximum count";
                truncateToMaximumCount();
            }
        };

    NX_VERBOSE(q) << "Live update request";

    m_liveFetch.id = getEvents(m_liveFetch.period, liveEventsReceived, Qt::DescendingOrder,
        m_liveFetch.batchSize);
}

rest::Handle EventSearchListModel::Private::getEvents(
    const QnTimePeriod& period, GetCallback callback, Qt::SortOrder order, int limit)
{
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(callback && server && server->restConnection());
    if (!callback || !server || !server->restConnection())
        return {};

    QnEventLogMultiserverRequestData request;
    request.filter.cameras = q->cameraSet()->type() != ManagedCameraSet::Type::all
        ? q->cameraSet()->cameras().toList()
        : QnVirtualCameraResourceList();

    request.filter.period = period;
    request.filter.eventType = m_selectedEventType;
    request.filter.eventSubtype = m_selectedSubType;
    request.limit = limit;
    request.order = order;

    NX_VERBOSE(q) << "Requesting events from"
        << utils::timestampToDebugString(period.startTimeMs) << "to"
        << utils::timestampToDebugString(period.endTimeMs()) << "in"
        << QVariant::fromValue(request.order).toString()
        << "maximum count" << request.limit;

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
    return m_helper->eventDetails(parameters).join("<br>");
}

QPixmap EventSearchListModel::Private::pixmap(const vms::event::EventParameters& parameters)
{
    switch (parameters.eventType)
    {
        case nx::vms::api::EventType::storageFailureEvent:
            return qnSkin->pixmap("events/storage_red.png");

        case nx::vms::api::EventType::backupFinishedEvent:
            return qnSkin->pixmap("events/storage_green.png");

        case nx::vms::api::EventType::serverStartEvent:
            return qnSkin->pixmap("events/server.png");

        case nx::vms::api::EventType::serverFailureEvent:
            return qnSkin->pixmap("events/server_red.png");

        case nx::vms::api::EventType::serverConflictEvent:
            return qnSkin->pixmap("events/server_yellow.png");

        case nx::vms::api::EventType::licenseIssueEvent:
            return qnSkin->pixmap("events/license_red.png");

        case nx::vms::api::EventType::cameraDisconnectEvent:
            return qnSkin->pixmap("events/connection_red.png");

        case nx::vms::api::EventType::networkIssueEvent:
        case nx::vms::api::EventType::cameraIpConflictEvent:
            return qnSkin->pixmap("events/connection_yellow.png");

        case nx::vms::api::EventType::softwareTriggerEvent:
            return QnSoftwareTriggerPixmaps::colorizedPixmap(
                parameters.description, QPalette().light().color());

        // TODO: #vkutin Fill with actual pixmaps as soon as they're created.
        case nx::vms::api::EventType::cameraMotionEvent:
        case nx::vms::api::EventType::cameraInputEvent:
            return qnSkin->pixmap("tree/camera.png");

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

} // namespace nx::client::desktop
