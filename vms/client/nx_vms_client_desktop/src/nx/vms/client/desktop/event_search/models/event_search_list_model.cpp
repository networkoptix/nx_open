// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_search_list_model.h"

#include <chrono>

#include <api/helpers/event_log_multiserver_request_data.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/event_search/models/detail/multi_request_id_holder.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/notification_levels.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::event;
using namespace std::chrono;

using FetchDirection = core::EventSearch::FetchDirection;
using MultiRequestIdHolder = core::detail::MultiRequestIdHolder;

struct Facade
{
    using Type = ActionData;
    using TimeType = microseconds;

    static auto startTime(const Type& data)
    {
        return microseconds(data.eventParams.eventTimestampUsec);
    }

    static const auto& id(const Type& data)
    {
        return data.actionParams.actionId;
    }

    static bool equal(const Type& /*left*/, const Type& /*right*/)
    {
        return false; //< Action data is unchangable.
    }
};

QString iconPath(const EventParameters& parameters)
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

        // TODO: #vkutin Fill with actual pixmaps as soon as they're created.
        case EventType::cameraInputEvent:
            return "tree/camera.svg";

        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
            // TODO:
            return {};

        default:
            return {};
    }
}

QColor color(const EventParameters& parameters)
{
    const auto color =
        QnNotificationLevel::notificationTextColor(QnNotificationLevel::valueOf(parameters));

    return color.isValid() ? color : QPalette().light().color();
}

bool hasPreview(EventType eventType)
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

QString eventTypesToString(const std::vector<EventType>& types)
{
    QStringList strList;
    for (const auto& type: types)
        strList.push_back(QString::fromStdString(nx::reflect::toString(type)));

    return strList.join(", ");
}

//-------------------------------------------------------------------------------------------------

struct EventSearchListModel::Private: public QObject
{
    EventSearchListModel* const q;

    const std::unique_ptr<nx::vms::event::StringsHelper> stringHelper;
    QTimer liveUpdateTimer;

    EventType selectedEventType = EventType::undefinedEvent;
    QString selectedSubType;
    std::vector<EventType> defaultEventTypes{EventType::anyEvent};

    ActionDataList data;

    MultiRequestIdHolder multiRequestIdHolder;

    Private(EventSearchListModel* q);

    QString title(const EventParameters& parameters) const;
    QString description(const EventParameters& parameters) const;

    using GetCallback = std::function<void(bool, rest::Handle, ActionDataList&&)>;
    rest::Handle getEvents(const core::FetchRequest& request,
        GetCallback callback) const;

    void fetchLive();

    bool requestFetch(const core::FetchRequest& request,
        const FetchCompletionHandler& completionHandler,
        MultiRequestIdHolder::Mode requestMode);
};

EventSearchListModel::Private::Private(EventSearchListModel* q):
    q(q),
    stringHelper(new vms::event::StringsHelper(q->systemContext()))
{
    static constexpr auto kLiveUpdateInterval = 15s;
    connect(&liveUpdateTimer, &QTimer::timeout, this, &Private::fetchLive);
    liveUpdateTimer.start(kLiveUpdateInterval);
}

QString EventSearchListModel::Private::title(const EventParameters& parameters) const
{
    return parameters.eventType == EventType::analyticsSdkEvent
        ? stringHelper->getAnalyticsSdkEventName(parameters)
        : stringHelper->eventName(parameters.eventType);
}

QString EventSearchListModel::Private::description(const EventParameters& parameters) const
{
    return stringHelper->eventDetails(parameters, event::AttrSerializePolicy::none)
        .join(common::html::kLineBreak);
}

rest::Handle EventSearchListModel::Private::getEvents(
    const core::FetchRequest& request,
    GetCallback callback) const
{
    if (!NX_ASSERT(callback && !q->isFilterDegenerate()))
        return {};

    QnEventLogMultiserverRequestData requestData;
    requestData.filter.cameras = q->cameraSet().type() != core::ManagedCameraSet::Type::all
        ? q->cameraSet().cameras().values()
        : QnVirtualCameraResourceList();

    requestData.filter.period = request.period(q->interestTimePeriod());
    requestData.order = core::EventSearch::sortOrderFromDirection(request.direction);
    requestData.filter.eventSubtype = selectedSubType;
    requestData.limit = q->maximumCount();
    requestData.filter.eventsOnly = true;

    if (selectedEventType == EventType::undefinedEvent)
        requestData.filter.eventTypeList = defaultEventTypes;
    else
        requestData.filter.eventTypeList.push_back(selectedEventType);

    NX_VERBOSE(q, "Requesting events:\n"
        "    from: %1\n    to: %2\n    type: %3\n    subtype: %4\n    sort: %5\n    limit: %6",
        nx::utils::timestampToDebugString(requestData.filter.period.startTimeMs),
        nx::utils::timestampToDebugString(requestData.filter.period.endTimeMs()),
        eventTypesToString(requestData.filter.eventTypeList),
        requestData.filter.eventSubtype,
        QVariant::fromValue(requestData.order).toString(),
        requestData.limit);

    const auto guardedCallback = nx::utils::guarded(this,
        [callback](bool success, rest::Handle handle, rest::EventLogData data)
        {
            callback(success, handle, std::move(data.data));
        });

    auto systemContext = appContext()->currentSystemContext();
    if (q->cameraSet().type() == core::ManagedCameraSet::Type::single
        && !q->cameraSet().cameras().empty())
    {
        systemContext = SystemContext::fromResource(*q->cameraSet().cameras().begin());
    }

    if (!NX_ASSERT(systemContext))
        return {};

    auto api = systemContext->connectedServerApi();
    if (!NX_ASSERT(api))
        return {};

    return api->getEvents(requestData, guardedCallback, q->thread());
}

void EventSearchListModel::Private::fetchLive()
{
    if (multiRequestIdHolder.value(MultiRequestIdHolder::Mode::dynamic)
        || !q->isLive()
        || !q->isOnline()
        || q->livePaused()
        || q->isFilterDegenerate())
    {
        return;
    }

    if (data.empty() && q->fetchInProgress())
        return; //< Don't fetch live if first fetch from archive is in progress.

    const auto request =
        [this]()
        {
            if (data.empty())
            {
                return core::FetchRequest{
                    .direction = FetchDirection::older,
                    .centralPointUs = microseconds(qnSyncTime->currentUSecsSinceEpoch())};
            }

            return core::FetchRequest{
                .direction = FetchDirection::newer,
                .centralPointUs = Facade::startTime(*data.begin())};
        }();

    requestFetch(request, {}, MultiRequestIdHolder::Mode::dynamic);
}

bool EventSearchListModel::Private::requestFetch(const core::FetchRequest& request,
    const FetchCompletionHandler& completionHandler,
    MultiRequestIdHolder::Mode requestMode)
{
    if (multiRequestIdHolder.value(requestMode))
        return false;

    const auto safeCompletionHandler =
        [completionHandler](const auto result, const auto& ranges, const auto& timeWindow)
    {
        if (completionHandler)
            completionHandler(result, ranges, timeWindow);
    };

    const auto handleLiveEventsReceived =
        [=](bool success, rest::Handle requestId, ActionDataList&& data)
        {
            if (!requestId || multiRequestIdHolder.value(requestMode) != requestId)
                return;

            multiRequestIdHolder.resetValue(requestMode);

            if (requestMode == MultiRequestIdHolder::Mode::dynamic && !q->isLive())
                return;

            // Ignore events later than current system time.
            core::truncateRawData<Facade>(data, request.direction, qnSyncTime->value());

            const auto applyFetchedData =
                 [this, request](core::FetchedData<ActionDataList>&& fetched,
                     const OptionalTimePeriod& timePeriod)
                {
                     // Explicitly update fetched time window in case of live update
                    q->setFetchedTimeWindow(timePeriod);
                    updateEventSearchData<Facade>(q, this->data, fetched, request.direction);
                };

            if (!success)
            {
                applyFetchedData({}, QnTimePeriod{});
                safeCompletionHandler(core::EventSearch::FetchResult::failed,
                    core::FetchedDataRanges{},
                    QnTimePeriod{});
                return;
            }

            auto fetched = core::makeFetchedData<Facade>(this->data, data, request);
            const auto ranges = fetched.ranges;
            const auto fetchedWindow = core::timeWindow<Facade>(fetched.data);
            applyFetchedData(std::move(fetched), fetchedWindow);
            safeCompletionHandler(core::EventSearch::FetchResult::complete, ranges, fetchedWindow);
        };

    const auto id = getEvents(request, handleLiveEventsReceived);
    multiRequestIdHolder.setValue(requestMode, id);
    return !!id;
}

//-------------------------------------------------------------------------------------------------

EventSearchListModel::EventSearchListModel(
    WindowContext* context,
    QObject* parent)
    :
    base_type(context->system(), parent),
    d(new Private(this))
{
    setLiveSupported(true);
    setLivePaused(true);
}

EventSearchListModel::~EventSearchListModel()
{
}

vms::api::EventType EventSearchListModel::selectedEventType() const
{
    return d->selectedEventType;
}

void EventSearchListModel::setSelectedEventType(vms::api::EventType value)
{
    if (value == d->selectedEventType)
        return;

    clear();
    d->selectedEventType = value;
    emit selectedEventTypeChanged();
}

QString EventSearchListModel::selectedSubType() const
{
    return d->selectedSubType;
}

void EventSearchListModel::setSelectedSubType(const QString& value)
{
    if (value == d->selectedSubType)
        return;

    clear();
    d->selectedSubType = value;
    emit selectedSubTypeChanged();
}

std::vector<nx::vms::api::EventType> EventSearchListModel::defaultEventTypes() const
{
    return d->defaultEventTypes;
}

void EventSearchListModel::setDefaultEventTypes(const std::vector<nx::vms::api::EventType>& value)
{
    d->defaultEventTypes = value;
    clear();
}

bool EventSearchListModel::isConstrained() const
{
    return selectedEventType() != vms::api::undefinedEvent
        || base_type::isConstrained();
}

bool EventSearchListModel::hasAccessRights() const
{
    return systemContext()->accessController()->hasGlobalPermissions(GlobalPermission::viewLogs);
}

int EventSearchListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid()
        ? 0
        : d->data.size();
}

QVariant EventSearchListModel::data(const QModelIndex& index, int role) const
{
    const auto& eventParams = d->data[index.row()].eventParams;

    switch (role)
    {
        case Qt::DisplayRole:
            return d->title(eventParams);

        case core::DecorationPathRole:
            return iconPath(eventParams);

        case Qt::DecorationRole:
        {
            if (eventParams.eventType == EventType::softwareTriggerEvent)
            {
                return QVariant::fromValue(SoftwareTriggerPixmaps::colorizedPixmap(
                    eventParams.description, QPalette().light().color()));
            }

            if (const auto path = index.data(core::DecorationPathRole).toString(); !path.isEmpty())
                return qnSkin->pixmap(path);

            return {};
        }

        case Qt::ForegroundRole:
            return QVariant::fromValue(color(eventParams));

        case core::DescriptionTextRole:
            return d->description(eventParams);

        case core::AnalyticsAttributesRole:
            return QVariant::fromValue(
                systemContext()->analyticsAttributeHelper()->preprocessAttributes(
                    eventParams.getAnalyticsObjectTypeId(), eventParams.attributes));

        case Qn::ForcePrecisePreviewRole:
            return hasPreview(eventParams.eventType)
                && eventParams.eventTimestampUsec > 0
                && eventParams.eventTimestampUsec != DATETIME_NOW;

        case core::PreviewTimeRole:
            if (!hasPreview(eventParams.eventType))
                return QVariant();
            [[fallthrough]];
        case core::TimestampRole:
            return QVariant::fromValue(microseconds(eventParams.eventTimestampUsec));
        case core::ObjectTrackIdRole:
            return QVariant::fromValue(eventParams.objectTrackId);

        case core::DisplayedResourceListRole:
        {
            if (eventParams.eventResourceId.isNull() && !eventParams.resourceName.isEmpty())
                return QVariant::fromValue(QStringList({eventParams.resourceName}));
        }
        [[fallthrough]];
        case core::ResourceListRole:
        {
            const auto resource =
                systemContext()->resourcePool()->getResourceById(eventParams.eventResourceId);
            if (resource)
                return QVariant::fromValue(QnResourceList({resource}));

            if (role == core::DisplayedResourceListRole)
                return {}; //< TODO: #vkutin Replace with <deleted camera> or <deleted server>.

            return {};
        }

        case core::ResourceRole: //< Resource for thumbnail preview only.
        {
            if (!hasPreview(eventParams.eventType))
                return false;

            return QVariant::fromValue<QnResourcePtr>(systemContext()->resourcePool()->
                getResourceById<QnVirtualCameraResource>(eventParams.eventResourceId));
        }

        case Qn::HelpTopicIdRole:
            return HelpTopic::Id::Empty;

        default:
            return {};
    }
}

bool EventSearchListModel::requestFetch(
    const core::FetchRequest& request,
    const FetchCompletionHandler& completionHandler)
{
    if (currentRequest())
        return false; //< Can't make a new request until the old one is in progress.

    // Can't make a new request until the old one is in progress.
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::dynamic);

    return d->requestFetch(request, completionHandler, MultiRequestIdHolder::Mode::fetch);
}

void EventSearchListModel::clearData()
{
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::fetch);
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::dynamic);
    d->data.clear();
}

} // namespace nx::vms::client::desktop
