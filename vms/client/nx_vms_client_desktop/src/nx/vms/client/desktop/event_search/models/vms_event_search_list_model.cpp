// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_event_search_list_model.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/std_helpers.h>
#include <nx/vms/api/rules/event_log_fwd.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/event_search/models/fetch_request.h>
#include <nx/vms/client/core/event_search/models/detail/multi_request_id_holder.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>
#include <nx/vms/client/desktop/event_search/models/private/event_model_data.h>
#include <nx/vms/client/desktop/event_search/utils/event_data.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/rules_fwd.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/field.h>
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
    using Type = EventModelData;
    using TimeType = milliseconds;

    static auto id(const Type& data)
    {
        // TODO: Add id field for the EventLogRecord structure and fill it with the database data.
        return QnUuid::fromArbitraryData(
            QString::number(data.record().timestampMs.count())
            + data.record().ruleId.toString());
    }

    static auto startTime(const Type& data)
    {
        return duration_cast<milliseconds>(data.record().timestampMs);
    }

    static bool equal(const Type& /*left*/, const Type& /*right*/)
    {
        return false; //< Data is unchangable.
    }

};

bool hasPreview(const EventPtr& event)
{
    return event->property(rules::utils::kCameraIdFieldName).isValid()
        || event->property(rules::utils::kDeviceIdsFieldName).isValid();
}

QString description(const QVariantMap& details)
{
    return details.value(rules::utils::kDetailingDetailName).toStringList()
        .join(common::html::kLineBreak);
}

QColor color(const QVariantMap& details)
{
    if (const auto level = details.value(rules::utils::kLevelDetailName); level.isValid())
    {
        return QnNotificationLevel::notificationTextColor(QnNotificationLevel::convert(
            level.value<nx::vms::event::Level>()));
    }

    return QPalette().light().color();
}

QPixmap iconPixmap(
    const EventPtr& event,
    const QVariantMap& details,
    QnResourcePool* pool)
{
    using nx::vms::event::Level;
    using nx::vms::rules::Icon;

    const auto icon = details.value(rules::utils::kIconDetailName).value<Icon>();
    const auto level = details.value(rules::utils::kLevelDetailName).value<Level>();
    const auto customIcon = details.value(rules::utils::kCustomIconDetailName).toString();

    QnResourceList devices;
    if (needIconDevices(icon))
    {
        devices = pool->getResourcesByIds<QnVirtualCameraResource>(
            rules::utils::getDeviceIds(rules::AggregatedEventPtr::create(event)));
    }

    return eventIcon(icon, level, customIcon, color(details), devices);
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct VmsEventSearchListModel::Private
{
    VmsEventSearchListModel* const q;

    const QScopedPointer<nx::vms::event::StringsHelper> helper;

    QString selectedEventType;
    QString selectedSubType;

    QStringList defaultEventTypes;

    std::deque<EventModelData> data;

    QTimer liveUpdateTimer;
    MultiRequestIdHolder multiRequestIdHolder;


    Private(VmsEventSearchListModel* q);

    void fetchLive();

    rest::Handle getEvents(
        const core::FetchRequest& request,
        GetCallback callback) const;

    bool requestFetch(
        const core::FetchRequest& request,
        const FetchCompletionHandler& completionHandler,
        MultiRequestIdHolder::Mode requestMode);
};

VmsEventSearchListModel::Private::Private(VmsEventSearchListModel* q):
    q(q),
    helper(new vms::event::StringsHelper(q->systemContext()))
{
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
                .centralPointUs = microseconds(qnSyncTime->currentUSecsSinceEpoch())};
        }

        return core::FetchRequest{
            .direction = FetchDirection::newer,
            .centralPointUs = Facade::startTime(*data.begin())};
    }();

    requestFetch(request, {}, MultiRequestIdHolder::Mode::dynamic);

}

rest::Handle VmsEventSearchListModel::Private::getEvents(
    const core::FetchRequest& request,
    GetCallback callback) const
{
    if (!NX_ASSERT(callback && !q->isFilterDegenerate()))
        return {};

    const auto timePointMs = duration_cast<milliseconds>(request.centralPointUs);
    using Period = nx::vms::api::ServerTimePeriod;
    nx::vms::api::rules::EventLogFilter filter = request.direction == FetchDirection::older
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
        filter.eventType = nx::toStdVector(defaultEventTypes);

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
            if (const auto error = std::get_if<nx::network::rest::Result>(&data))
            {
                NX_WARNING(this, "Event log request: %1 failed: %2, %3",
                    handle, error->error, error->errorString);

                callback(false, handle, {});
            }
            else
            {
                callback(success, handle, std::get<EventLogRecordList>(std::move(data)));
            }
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

bool VmsEventSearchListModel::Private::requestFetch(const core::FetchRequest& request,
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
        [=](bool success, rest::Handle requestId, EventLogRecordList&& data)
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

VmsEventSearchListModel::VmsEventSearchListModel(WindowContext* context, QObject* parent):
    base_type(context->system(), parent),
    d(new Private(this))
{
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

const QStringList& VmsEventSearchListModel::defaultEventTypes() const
{
    return d->defaultEventTypes;
}

void VmsEventSearchListModel::setDefaultEventTypes(const QStringList& value)
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

    const auto& event = d->data[index.row()].event(systemContext());
    const auto& details = event->details(systemContext());

    switch (role)
    {
        case Qt::DisplayRole:
            return eventTitle(details);

        case Qt::DecorationRole:
            return iconPixmap(event, details, resourcePool());

        case Qt::ForegroundRole:
            return QVariant::fromValue(color(details));

        case core::DescriptionTextRole:
            return description(details);

        case core::AnalyticsAttributesRole:
            if (const auto attrVal = event->property("attributes");
                attrVal.canConvert<nx::common::metadata::Attributes>())
            {
                return QVariant::fromValue(
                    systemContext()->analyticsAttributeHelper()->preprocessAttributes(
                        event->property("objectTypeId").toString(),
                        attrVal.value<nx::common::metadata::Attributes>()));
            }
            else
            {
                return {};
            }

        case Qn::ForcePrecisePreviewRole:
            return hasPreview(event)
                && event->timestamp() > microseconds::zero()
                && event->timestamp().count() != DATETIME_NOW;

        case core::PreviewTimeRole:
            if (!hasPreview(event))
                return QVariant();
        [[fallthrough]];
        case core::TimestampRole:
            return QVariant::fromValue(event->timestamp()); //< Microseconds.

        case core::ObjectTrackIdRole:
            return event->property("objectTrackId");

        case Qn::DisplayedResourceListRole:
            if (const auto sourceName = details.value(rules::utils::kSourceNameDetailName);
                sourceName.isValid())
            {
                return QVariant::fromValue(QStringList(sourceName.toString()));
            }
        [[fallthrough]];
        case Qn::ResourceListRole:
        {
            if (const auto sourceId = details.value(rules::utils::kSourceIdDetailName);
                sourceId.isValid())
            {
                if (const auto resource =
                    resourcePool()->getResourceById(sourceId.value<QnUuid>()))
                {
                    return QVariant::fromValue(QnResourceList({resource}));
                }

                if (role == Qn::DisplayedResourceListRole)
                    return {}; //< TODO: #vkutin Replace with <deleted camera> or <deleted server>.
            }

            return {};
        }

        case core::ResourceRole: //< Resource for thumbnail preview only.
        {
            if (!hasPreview(event))
                return {};

            return QVariant::fromValue<QnResourcePtr>(
                resourcePool()->getResourceById<QnVirtualCameraResource>(
                    details.value(rules::utils::kSourceIdDetailName).value<QnUuid>()));
        }

        case Qn::HelpTopicIdRole:
            return HelpTopic::Id::Empty;

        default:
            return {};
    }
}

bool VmsEventSearchListModel::requestFetch(
    const core::FetchRequest& request,
    const FetchCompletionHandler& completionHandler)
{
    if (currentRequest())
        return false; //< Can't make a new request until the old one is in progress.

    // Can't make a new request until the old one is in progress.
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::dynamic);

    return d->requestFetch(request, completionHandler, MultiRequestIdHolder::Mode::fetch);
}

void VmsEventSearchListModel::clearData()
{
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::fetch);
    d->multiRequestIdHolder.resetValue(MultiRequestIdHolder::Mode::dynamic);
    d->data.clear();
}

} // namespace nx::vms::client::desktop
