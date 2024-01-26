// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_search_list_model_p.h"

#include <QtCore/QTimer>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <analytics/common/object_metadata.h>
#include <api/helpers/event_log_multiserver_request_data.h>
#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/datetime.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/event/strings_helper.h>
#include <ui/common/notification_levels.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using nx::vms::api::EventType;
using nx::vms::api::ActionType;
using nx::vms::event::ActionData;
using nx::vms::event::ActionDataList;

using namespace std::chrono;

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

void truncate(ActionDataList& data, AbstractSearchListModel::FetchDirection direction,
    milliseconds maxTimestamp)
{
    if (direction == AbstractSearchListModel::FetchDirection::earlier)
    {
        const auto end = std::lower_bound(data.begin(), data.end(), maxTimestamp,
            lowerBoundPredicate);

        data.erase(data.begin(), end);
    }
    else if (NX_ASSERT(direction == AbstractSearchListModel::FetchDirection::later))
    {
        const auto begin = std::lower_bound(data.rbegin(), data.rend(), maxTimestamp,
            lowerBoundPredicate).base();

        data.erase(begin, data.end());
    }
}

QString eventTypesToString(const std::vector<EventType>& types)
{
    QStringList strList;
    for (const auto& type: types)
        strList.push_back(QString::fromStdString(nx::reflect::toString(type)));

    return strList.join(", ");
};

} // namespace

EventSearchListModel::Private::Private(EventSearchListModel* q):
    base_type(q),
    q(q),
    m_helper(new vms::event::StringsHelper(q->system())),
    m_liveUpdateTimer(new QTimer())
{
    connect(m_liveUpdateTimer.data(), &QTimer::timeout, this, &Private::fetchLive);
    m_liveUpdateTimer->start(kLiveUpdateInterval);
}

EventSearchListModel::Private::~Private()
{
}

EventType EventSearchListModel::Private::selectedEventType() const
{
    return m_selectedEventType;
}

void EventSearchListModel::Private::setSelectedEventType(EventType value)
{
    if (m_selectedEventType == value)
        return;

    q->clear();
    m_selectedEventType = value;
    emit q->selectedEventTypeChanged();
}

QString EventSearchListModel::Private::selectedSubType() const
{
    return m_selectedSubType;
}

void EventSearchListModel::Private::setSelectedSubType(const QString& value)
{
    if (m_selectedSubType == value)
        return;

    q->clear();
    m_selectedSubType = value;
    emit q->selectedSubTypeChanged();
}

const std::vector<EventType>& EventSearchListModel::Private::defaultEventTypes() const
{
    return m_defaultEventTypes;
}

void EventSearchListModel::Private::setDefaultEventTypes(const std::vector<EventType>& value)
{
    m_defaultEventTypes = value;
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
            return title(eventParams);

        case Qn::DecorationPathRole:
            return iconPath(eventParams);

        case Qt::DecorationRole:
            if (eventParams.eventType == EventType::softwareTriggerEvent)
            {
                return QVariant::fromValue(SoftwareTriggerPixmaps::colorizedPixmap(
                    eventParams.description, QPalette().light().color()));
            }
            handled = false;
            return {};

        case Qt::ForegroundRole:
            return QVariant::fromValue(color(eventParams));

        case Qn::DescriptionTextRole:
            return description(eventParams);

        case Qn::AnalyticsAttributesRole:
            return QVariant::fromValue(
                qnClientModule->analyticsAttributeHelper()->preprocessAttributes(
                    eventParams.getAnalyticsObjectTypeId(), eventParams.attributes));

        case Qn::ForcePrecisePreviewRole:
            return hasPreview(eventParams.eventType)
                && eventParams.eventTimestampUsec > 0
                && eventParams.eventTimestampUsec != DATETIME_NOW;

        case Qn::PreviewTimeRole:
            if (!hasPreview(eventParams.eventType))
                return QVariant();
            [[fallthrough]];
        case Qn::TimestampRole:
            return QVariant::fromValue(microseconds(eventParams.eventTimestampUsec));
        case Qn::ObjectTrackIdRole:
            return QVariant::fromValue(eventParams.objectTrackId);

        case Qn::DisplayedResourceListRole:
        {
            if (eventParams.eventResourceId.isNull() && !eventParams.resourceName.isEmpty())
                return QVariant::fromValue(QStringList({eventParams.resourceName}));
        }
        [[fallthrough]];
        case Qn::ResourceListRole:
        {
            const auto resource = q->system()->resourcePool()->getResourceById(
                eventParams.eventResourceId);
            if (resource)
                return QVariant::fromValue(QnResourceList({resource}));

            if (role == Qn::DisplayedResourceListRole)
                return {}; //< TODO: #vkutin Replace with <deleted camera> or <deleted server>.

            return {};
        }

        case Qn::ResourceRole: //< Resource for thumbnail preview only.
        {
            if (!hasPreview(eventParams.eventType))
                return false;

            return QVariant::fromValue<QnResourcePtr>(q->system()->resourcePool()->
                getResourceById<QnVirtualCameraResource>(eventParams.eventResourceId));
        }

        case Qn::HelpTopicIdRole:
            return HelpTopic::Id::Empty;

        default:
            handled = false;
            return {};
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
    if (!q->truncateDataToMaximumCount(m_data, &startTime))
        return;

    if (q->fetchDirection() == FetchDirection::earlier)
        q->setLive(false);
}

void EventSearchListModel::Private::truncateToRelevantTimePeriod()
{
    q->truncateDataToTimePeriod(m_data, &startTime, q->relevantTimePeriod());
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
                // Ignore events later than current system time.
                truncate(data, currentRequest().direction, qnSyncTime->value());

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

            // Trying to deduce if we found exactly the same event which is displayed last.
            // Probably we should remove the last millisecond events from m_data and append all
            // new ones.
            const ActionData& data =*iter;
            if (timeUs == lastTimeUs && data.eventParams == last.eventParams)
                break;
        }
    }

    const auto count = std::distance(begin, end);
    if (count <= 0)
    {
        NX_VERBOSE(q, "Committing no events");
        return false;
    }

    NX_VERBOSE(q, "Committing %1 events:\n    from: %2\n    to: %3", count,
        nx::utils::timestampToDebugString(startTime(*(end - 1)).count()),
        nx::utils::timestampToDebugString(startTime(*begin).count()));

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
    if (m_liveFetch.id || !q->isLive() || !q->isOnline() || q->livePaused() || q->isFilterDegenerate())
        return;

    if (m_data.empty() && fetchInProgress())
        return; //< Don't fetch live if first fetch from archive is in progress.

    const milliseconds from = (m_data.empty() ? 0ms : startTime(m_data.front()));
    m_liveFetch.period = QnTimePeriod(from.count(), QnTimePeriod::kInfiniteDuration);
    m_liveFetch.direction = FetchDirection::later;
    m_liveFetch.batchSize = q->fetchBatchSize();

    const auto liveEventsReceived =
        [this](bool success, rest::Handle requestId, ActionDataList&& data)
        {
            const auto scopedClear = nx::utils::makeScopeGuard([this]() { m_liveFetch = {}; });

            if (!success || !q->isLive() || !requestId || requestId != m_liveFetch.id)
                return;

            // Ignore events later than current system time.
            truncate(data, m_liveFetch.direction, qnSyncTime->value());

            if (data.empty())
                return;

            auto periodToCommit = QnTimePeriod::fromInterval(
                startTime(data.back()), startTime(data.front()));

            ScopedLiveCommit liveCommit(q);

            q->addToFetchedTimeWindow(periodToCommit);

            NX_VERBOSE(q, "Live update commit");
            commitInternal(periodToCommit, data.rbegin(), data.rend(), 0, true);

            if (count() > q->maximumCount())
            {
                NX_VERBOSE(q, "Truncating to maximum count");
                truncateToMaximumCount();
            }
        };

    NX_VERBOSE(q, "Live update request");

    m_liveFetch.id = getEvents(m_liveFetch.period, liveEventsReceived, Qt::AscendingOrder,
        m_liveFetch.batchSize);
}

rest::Handle EventSearchListModel::Private::getEvents(
    const QnTimePeriod& period, GetCallback callback, Qt::SortOrder order, int limit) const
{
    if (!NX_ASSERT(callback && !q->isFilterDegenerate()))
        return {};

    QnEventLogMultiserverRequestData request;
    request.filter.cameras = q->cameraSet()->type() != ManagedCameraSet::Type::all
        ? q->cameraSet()->cameras().values()
        : QnVirtualCameraResourceList();

    request.filter.period = period;
    request.filter.eventSubtype = m_selectedSubType;
    request.limit = limit;
    request.order = order;
    request.filter.eventsOnly = true;

    if (m_selectedEventType == EventType::undefinedEvent)
        request.filter.eventTypeList = m_defaultEventTypes;
    else
        request.filter.eventTypeList.push_back(m_selectedEventType);

    NX_VERBOSE(q, "Requesting events:\n"
        "    from: %1\n    to: %2\n    type: %3\n    subtype: %4\n    sort: %5\n    limit: %6",
        nx::utils::timestampToDebugString(period.startTimeMs),
        nx::utils::timestampToDebugString(period.endTimeMs()),
        eventTypesToString(request.filter.eventTypeList),
        request.filter.eventSubtype,
        QVariant::fromValue(request.order).toString(),
        request.limit);

    const auto internalCallback =
        [callback, guard = QPointer<const Private>(this)](
            bool success, rest::Handle handle, rest::EventLogData data)
        {
            if (guard)
                callback(success, handle, std::move(data.data));
        };

    auto systemContext = appContext()->currentSystemContext();
    if (q->cameraSet()->type() == ManagedCameraSet::Type::single
        && !q->cameras().empty())
    {
        systemContext = SystemContext::fromResource(*q->cameras().begin());
    }

    if (!NX_ASSERT(systemContext))
        return {};

    auto api = systemContext->connectedServerApi();
    if (!NX_ASSERT(api))
        return {};

    return api->getEvents(request, internalCallback, thread());
}

QString EventSearchListModel::Private::title(const vms::event::EventParameters& parameters) const
{
    return parameters.eventType == EventType::analyticsSdkEvent
        ? m_helper->getAnalyticsSdkEventName(parameters)
        : m_helper->eventName(parameters.eventType);
}

QString EventSearchListModel::Private::description(
    const vms::event::EventParameters& parameters) const
{
    using namespace nx::vms;
    return m_helper->eventDetails(parameters, event::AttrSerializePolicy::none)
        .join(common::html::kLineBreak);
}

QString EventSearchListModel::Private::iconPath(const vms::event::EventParameters& parameters)
{
    switch (parameters.eventType)
    {
        case EventType::storageFailureEvent:
            return "events/storage_red.png";

        case EventType::backupFinishedEvent:
            return "events/storage_green.png";

        case EventType::serverStartEvent:
            return "events/server_20.svg";

        case EventType::serverFailureEvent:
        case EventType::serverCertificateError:
            return "events/server_red.png";

        case EventType::serverConflictEvent:
            return "events/server_yellow.png";

        case EventType::licenseIssueEvent:
            return "events/license_red.png";

        case EventType::cameraDisconnectEvent:
            return "events/connection_red.png";

        case EventType::networkIssueEvent:
        case EventType::cameraIpConflictEvent:
            return "events/connection_yellow.png";

        case EventType::softwareTriggerEvent:
            return SoftwareTriggerPixmaps::pixmapsPath() + parameters.description + ".png";

        case EventType::pluginDiagnosticEvent:
        {
            switch (QnNotificationLevel::valueOf(parameters))
            {
                case QnNotificationLevel::Value::CriticalNotification:
                    return "events/alert_red.png";
                case QnNotificationLevel::Value::ImportantNotification:
                    return "events/alert_yellow.png";
                default:
                    return "events/alert_20.svg";
            }
        }

        case EventType::cameraMotionEvent:
            return "events/motion.svg";

        // TODO: #spanasenko Fill with actual pixmaps as soon as they're created.
        case EventType::cameraInputEvent:
        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
            return "tree/camera.svg";

        default:
            return {};
    }
}

QColor EventSearchListModel::Private::color(const vms::event::EventParameters& parameters)
{
    const auto color =
        QnNotificationLevel::notificationTextColor(QnNotificationLevel::valueOf(parameters));

    return color.isValid() ? color : QPalette().light().color();
}

bool EventSearchListModel::Private::hasPreview(EventType eventType)
{
    switch (eventType)
    {
        case EventType::cameraMotionEvent:
        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
        case EventType::cameraInputEvent:
        case EventType::userDefinedEvent:
        case EventType::softwareTriggerEvent:
        case EventType::cameraDisconnectEvent:
        case EventType::networkIssueEvent:
            return true;

        default:
            return false;
    }
}

} // namespace nx::vms::client::desktop
