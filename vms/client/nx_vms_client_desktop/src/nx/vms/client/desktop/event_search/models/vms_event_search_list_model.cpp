// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_event_search_list_model.h"

#include <QtCore/QTimer>
#include <QtGui/QPalette>

#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/std_helpers.h>
#include <nx/vms/api/rules/event_log_fwd.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/detail/multi_request_id_holder.h>
#include <nx/vms/client/core/event_search/models/fetch_request.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/utils/log_strings_format.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/event_search/models/private/event_model_data.h>
#include <nx/vms/client/desktop/event_search/utils/event_data.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/rules/helpers/help_id.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/rules_fwd.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/event_log.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/resource.h>
#include <ui/common/notification_levels.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using EventPtr = nx::vms::rules::EventPtr;
using EventLogRecordList = nx::vms::api::rules::EventLogRecordList;
using FetchDirection = nx::vms::client::core::EventSearch::FetchDirection;
using MultiRequestIdHolder = core::detail::MultiRequestIdHolder;
using GetCallback = std::function<void(bool, rest::Handle, EventLogRecordList&&)>;

using namespace std::chrono;

namespace {

struct Facade
{
    using Type = nx::vms::api::rules::EventLogRecord;
    using TimeType = milliseconds;
    using IdType = nx::Uuid;

    static auto id(const Type& data)
    {
        // We have different event types, related to the server or camera with different
        // parameters. Each distinct event is characterized by it's event data so we can rely
        // on it when creating unique identifier of the event.
        return nx::Uuid::fromArbitraryData(QJson::serialized(data.eventData));
    }

    static auto startTime(const Type& data)
    {
        return data.timestampMs;
    }

    static bool equal(const Type& /*left*/, const Type& /*right*/)
    {
        return false; //< Data is unchangeable.
    }
};

bool hasPreview(const QVariantMap& details)
{
    return nx::vms::rules::utils::EventLog::sourceResourceType(details)
        == nx::vms::rules::ResourceType::device;
}

QnResourceList accessibleDevices(core::SystemContext* context, const UuidList& deviceIds)
{
    return context->resourcePool()->getResourcesByIds(deviceIds).filtered(
        [accessController = context->accessController()](const QnResourcePtr& resource)
        {
            return accessController->hasPermissions(resource, Qn::ViewContentPermission);
        });
}

QColor color(const QVariantMap& details)
{
    if (const auto level = details.value(rules::utils::kLevelDetailName); level.isValid())
    {
        return QnNotificationLevel::notificationTextColor(level.value<nx::vms::event::Level>());
    }

    return QPalette().light().color();
}

QString iconPath(
    const nx::vms::rules::AggregatedEventPtr& event,
    const QVariantMap& details,
    QnResourcePool* pool)
{
    using nx::vms::event::Level;
    using nx::vms::rules::Icon;

    const auto icon = details.value(rules::utils::kIconDetailName).value<Icon>();
    const auto customIcon = details.value(rules::utils::kCustomIconDetailName).toString();

    QnResourceList devices;
    if (needIconDevices(icon))
    {
        devices = pool->getResourcesByIds<QnVirtualCameraResource>(
            rules::utils::getDeviceIds(event));
    }

    return eventIconPath(icon, customIcon, devices);
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct VmsEventSearchListModel::Private
{
    VmsEventSearchListModel* const q;

    QString selectedEventType;
    QString selectedSubType;

    std::vector<QString> defaultEventTypes;

    std::deque<nx::vms::api::rules::EventLogRecord> data;

    QTimer liveUpdateTimer;
    MultiRequestIdHolder multiRequestIdHolder;

    Private(VmsEventSearchListModel* q);

    void fetchLive();

    rest::Handle getEvents(const core::FetchRequest& request, GetCallback callback) const;

    bool requestFetch(
        const core::FetchRequest& request,
        const FetchCompletionHandler& completionHandler,
        MultiRequestIdHolder::Mode requestMode);
};

VmsEventSearchListModel::Private::Private(VmsEventSearchListModel* q):
    q(q)
{
    static constexpr auto kLiveUpdateInterval = 15s;

    liveUpdateTimer.callOnTimeout([this]{ fetchLive(); });
    liveUpdateTimer.start(kLiveUpdateInterval);
}

void VmsEventSearchListModel::Private::fetchLive()
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
                .centralPointUs = microseconds(qnSyncTime->currentUSecsSinceEpoch()),
                .liveUpdate = true};
        }

        return core::FetchRequest{
            .direction = FetchDirection::newer,
            .centralPointUs = Facade::startTime(*data.begin()),
            .liveUpdate = true};
    }();

    requestFetch(request, {}, MultiRequestIdHolder::Mode::dynamic);
}

rest::Handle VmsEventSearchListModel::Private::getEvents(
    const core::FetchRequest& request,
    GetCallback callback) const
{
    NX_CRITICAL(callback);
    if (!NX_ASSERT(!q->isFilterDegenerate()))
        return {};

    const auto timePointMs = duration_cast<milliseconds>(request.centralPointUs);
    using Period = nx::vms::api::ServerTimePeriod;
    nx::vms::api::rules::EventLogFilter filter = (request.direction == FetchDirection::older)
        ? Period{.startTimeMs = Period::kMinTimeValue, .durationMs = timePointMs}
        : Period{.startTimeMs = timePointMs};

    if (q->cameraSet().type() != core::ManagedCameraSet::Type::all)
    {
        filter.eventResourceId = std::vector<QString>();

        for (const auto& camera: q->cameraSet().cameras())
            filter.eventResourceId->valueOrArray.push_back(camera->getId().toSimpleString());
    }

    filter.eventSubtype = selectedSubType;
    filter.limit = q->maximumCount();
    filter.order = core::EventSearch::sortOrderFromDirection(request.direction);
    filter.eventsOnly = true;

    // TODO: #amalov Fix default event types.
    if (!selectedEventType.isEmpty())
        filter.eventType = {selectedEventType};
    else if (!defaultEventTypes.empty())
        filter.eventType = defaultEventTypes;

    NX_VERBOSE(q, "Requesting events:\n"
        "    from: %1\n    to: %2\n    type: %3\n    subtype: %4\n    sort: %5\n    limit: %6",
        nx::utils::timestampToDebugString(filter.startTimeMs),
        nx::utils::timestampToDebugString(filter.endTime()),
        filter.eventType.value_or(std::vector<QString>()).valueOrArray,
        filter.eventSubtype,
        filter.order,
        filter.limit);

    const auto internalCallback = nx::utils::guarded(q,
        [this, callback = std::move(callback)](
            bool success, rest::Handle handle, rest::ErrorOrData<EventLogRecordList>&& data)
        {
            NX_LOG_RESPONSE(this, success, data, "Event log request %1 failed.", handle);

            if (!data)
                callback(false, handle, {});
            else
                callback(success, handle, std::move(*data));
        });

    auto systemContext = q->systemContext();
    const auto& cameraSet = q->cameraSet();
    if (cameraSet.type() == core::ManagedCameraSet::Type::single)
    {
        if (const auto& cameras = cameraSet.cameras(); !cameras.empty())
            systemContext = SystemContext::fromResource(*cameras.begin());
    }

    if (!NX_ASSERT(systemContext))
        return {};

    const auto api = systemContext->connectedServerApi();
    if (!NX_ASSERT(api))
        return {};

    return api->eventLog(filter, std::move(internalCallback), q->thread());
}

bool VmsEventSearchListModel::Private::requestFetch(
    const core::FetchRequest& request,
    const FetchCompletionHandler& completionHandler,
    MultiRequestIdHolder::Mode requestMode)
{
    const bool inProgress = multiRequestIdHolder.value(requestMode);

    NX_VERBOSE(this, "Fetch request, dir: %1, center: %2, mode: %3, in progress: %4",
        request.direction, request.centralPointUs, requestMode, inProgress);

    if (inProgress)
        return false;

    const auto safeCompletionHandler =
        [completionHandler](const auto result, const auto& ranges, const auto& timeWindow)
    {
        if (completionHandler)
            completionHandler(result, ranges, timeWindow);
    };

    const auto handleLiveEventsReceived =
        [=, this](bool success, rest::Handle requestId, EventLogRecordList&& data)
    {
        if (!requestId || multiRequestIdHolder.value(requestMode) != requestId)
            return;

        multiRequestIdHolder.resetValue(requestMode);

        if (requestMode == MultiRequestIdHolder::Mode::dynamic && !q->isLive())
            return;

        // Ignore events later than current system time.
        core::truncateRawData<Facade>(data, request.direction, qnSyncTime->value());

        const auto applyFetchedData =
            [this, request](core::FetchedData<EventLogRecordList>&& fetched,
                const OptionalTimePeriod& timePeriod)
        {
            // Explicitly update fetched time window in case of live update
            q->setFetchedTimeWindow(timePeriod);
            updateEventSearchData<Facade>(q, this->data, fetched, request.direction);
        };

        if (!success)
        {
            applyFetchedData({}, QnTimePeriod{});
            safeCompletionHandler(
                core::EventSearch::FetchResult::failed,
                core::FetchedDataRanges{},
                QnTimePeriod{});
            return;
        }

        // We can have here duplicated events as the server returns the list of occurred actions.
        // So the trick is to leave only unique events and as we generate unique id for each event
        // we can just remove duplicates here.
        core::removeDuplicateItems<Facade>(data, data.begin(), data.end(), request.direction);

        core::mergeOldData<Facade>(data, this->data, request.direction);
        auto fetched = core::makeFetchedData<Facade>(this->data, data, request);
        const auto ranges = fetched.ranges;
        const auto fetchedWindow = core::timeWindow<Facade>(fetched.data);
        emit q->fetchCommitStarted(request);
        applyFetchedData(std::move(fetched), fetchedWindow);
        safeCompletionHandler(core::EventSearch::FetchResult::complete, ranges, fetchedWindow);
    };

    const auto id = getEvents(request, handleLiveEventsReceived);
    multiRequestIdHolder.setValue(requestMode, id);
    return !!id;
}
//-------------------------------------------------------------------------------------------------

VmsEventSearchListModel::VmsEventSearchListModel(WindowContext* context, QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    setSystemContext(context->system());
    setLiveSupported(true);
    setLivePaused(true);
}

VmsEventSearchListModel::~VmsEventSearchListModel()
{
}

QString VmsEventSearchListModel::selectedEventType() const
{
    return d->selectedEventType;
}

void VmsEventSearchListModel::setSelectedEventType(const QString& value)
{
    if (d->selectedEventType == value)
        return;

    clear();
    d->selectedEventType = value;
    emit selectedEventTypeChanged();
}

QString VmsEventSearchListModel::selectedSubType() const
{
    return d->selectedSubType;
}

void VmsEventSearchListModel::setSelectedSubType(const QString& value)
{
    if (d->selectedSubType == value)
        return;

    clear();
    d->selectedSubType = value;
    emit selectedSubTypeChanged();
}

const std::vector<QString>& VmsEventSearchListModel::defaultEventTypes() const
{
    return d->defaultEventTypes;
}

void VmsEventSearchListModel::setDefaultEventTypes(const std::vector<QString>& value)
{
    d->defaultEventTypes = value;
    clear();
}

bool VmsEventSearchListModel::isConstrained() const
{
    return !selectedEventType().isEmpty() || base_type::isConstrained();
}

bool VmsEventSearchListModel::hasAccessRights() const
{
    if (!NX_ASSERT(systemContext()))
        return false;

    return systemContext()->accessController()->hasGlobalPermissions(GlobalPermission::viewLogs);
}

int VmsEventSearchListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid()
        ? 0
        : d->data.size();
}

QVariant VmsEventSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(index.row() < static_cast<int>(d->data.size())))
        return {};

    if (!NX_ASSERT(systemContext()))
        return {};

    using namespace nx::vms::rules::utils;

    const auto infoLevel = appContext()->localSettings()->resourceInfoLevel();
    const auto event = nx::vms::rules::AggregatedEventPtr::create(
        systemContext()->vmsRulesEngine(),
        d->data[index.row()]);
    const auto& details = event->details(systemContext(), infoLevel);

    switch (role)
    {
        case Qt::DisplayRole:
            return EventLog::caption(details);

        case core::DecorationPathRole:
            return iconPath(event, details, systemContext()->resourcePool());

        case Qt::ForegroundRole:
            return QVariant::fromValue(color(details));

        case core::DescriptionTextRole:
            return EventLog::tileDescription(details);

        case Qt::ToolTipRole:
            return EventLog::descriptionTooltip(event, systemContext(), infoLevel);

        case core::AnalyticsAttributesRole:
            if (const auto attrVal = event->property(kAttributesFieldName);
                attrVal.canConvert<nx::common::metadata::Attributes>())
            {
                return QVariant::fromValue(
                    systemContext()->analyticsAttributeHelper()->preprocessAttributes(
                        event->property(kObjectTypeIdFieldName).toString(),
                        attrVal.value<nx::common::metadata::Attributes>()));
            }
            else
            {
                return {};
            }

        case core::ForcePrecisePreviewRole:
            return hasPreview(details)
                && event->timestamp() > microseconds::zero()
                && event->timestamp().count() != DATETIME_NOW;

        case core::PreviewTimeRole:
            if (!hasPreview(details))
                return QVariant();
        [[fallthrough]];
        case core::TimestampRole:
            return QVariant::fromValue(event->timestamp()); //< Microseconds.

        case core::ObjectTrackIdRole:
            return event->property(kObjectTrackIdFieldName);

        case core::ItemZoomRectRole:
            return event->property(kBoundingBoxFieldName);

        // TODO: #sivanov Replace with a corresponding function when the event search unit
        // tests are ready.
        case core::DisplayedResourceListRole: //< Event tile source label.
        {
            return QVariant::fromValue<QStringList>({
                nx::vms::rules::utils::EventLog::sourceText(systemContext(), details)});
        }

        case core::ResourceListRole: //< Event tooltip thumbnails.
        {
            return QVariant::fromValue(accessibleDevices(
                systemContext(), EventLog::sourceDeviceIds(details)));
        }

        case core::ResourceRole: //< Resource for tile thumbnail preview only.
        {
            auto devices = accessibleDevices(systemContext(), EventLog::sourceDeviceIds(details));

            if (devices.count() == 1)
                return QVariant::fromValue(devices.front());

            return {};
        }

        case Qn::HelpTopicIdRole:
            return eventHelpId(event->type());

        default:
            return base_type::data(index, role);
    }
}

bool VmsEventSearchListModel::requestFetch(
    const core::FetchRequest& request,
    const FetchCompletionHandler& completionHandler)
{
    // Cancel dynamic update request.
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::dynamic);

    return d->requestFetch(request, completionHandler, MultiRequestIdHolder::Mode::fetch);
}

void VmsEventSearchListModel::clearData()
{
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::fetch);
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::dynamic);

    ScopedModelOperations::ScopedReset reset(this);
    d->data.clear();
}

} // namespace nx::vms::client::desktop
