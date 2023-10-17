// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_event_search_list_model_p.h"

#include <QtCore/QTimer>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <analytics/common/object_metadata.h>
#include <api/helpers/event_log_multiserver_request_data.h>
#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <client/client_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/datetime.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/event_search/utils/event_data.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/common/notification_levels.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::vms::rules;
using namespace nx::vms::api::rules;

namespace {

constexpr auto kLiveUpdateInterval = 15s;

milliseconds startTime(const EventModelData& data)
{
    return duration_cast<milliseconds>(data.record().timestampMs);
}

bool lowerBoundPredicate(const EventModelData& left, milliseconds right)
{
    return startTime(left) > right;
}

bool upperBoundPredicate(milliseconds left, const EventModelData& right)
{
    return left > startTime(right);
}

void truncate(
    EventLogRecordList& data,
    AbstractSearchListModel::FetchDirection direction,
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

} // namespace

VmsEventSearchListModel::Private::Private(VmsEventSearchListModel* q):
    base_type(q),
    q(q),
    m_helper(new vms::event::StringsHelper(q->system())),
    m_liveUpdateTimer(new QTimer())
{
    connect(m_liveUpdateTimer.data(), &QTimer::timeout, this, &Private::fetchLive);
    m_liveUpdateTimer->start(kLiveUpdateInterval);
}

VmsEventSearchListModel::Private::~Private()
{
}

QString VmsEventSearchListModel::Private::selectedEventType() const
{
    return m_selectedEventType;
}

void VmsEventSearchListModel::Private::setSelectedEventType(const QString& value)
{
    if (m_selectedEventType == value)
        return;

    q->clear();
    m_selectedEventType = value;
    emit q->selectedEventTypeChanged();
}

QString VmsEventSearchListModel::Private::selectedSubType() const
{
    return m_selectedSubType;
}

void VmsEventSearchListModel::Private::setSelectedSubType(const QString& value)
{
    if (m_selectedSubType == value)
        return;

    q->clear();
    m_selectedSubType = value;
    emit q->selectedSubTypeChanged();
}

const QStringList& VmsEventSearchListModel::Private::defaultEventTypes() const
{
    return m_defaultEventTypes;
}

void VmsEventSearchListModel::Private::setDefaultEventTypes(const QStringList& value)
{
    m_defaultEventTypes = value;
    q->clear();
}

int VmsEventSearchListModel::Private::count() const
{
    return int(m_data.size());
}

QVariant VmsEventSearchListModel::Private::data(
    const QModelIndex& index,
    int role,
    bool& handled) const
{
    if (!NX_ASSERT(index.row() < m_data.size()))
        return {};

    const auto& event = m_data[index.row()].event(q->system());
    const auto& details = event->details(q->system());
    handled = true;

    switch (role)
    {
        case Qt::DisplayRole:
            return eventTitle(details);

        case Qt::DecorationRole:
            return iconPixmap(event, details);

        case Qt::ForegroundRole:
            return QVariant::fromValue(color(details));

        case Qn::DescriptionTextRole:
            return description(details);

        case Qn::AnalyticsAttributesRole:
            if (const auto attrVal = event->property("attributes");
                attrVal.canConvert<nx::common::metadata::Attributes>())
            {
                return QVariant::fromValue(
                    qnClientModule->analyticsAttributeHelper()->preprocessAttributes(
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

        case Qn::PreviewTimeRole:
            if (!hasPreview(event))
                return QVariant();
        [[fallthrough]];
        case Qn::TimestampRole:
            return QVariant::fromValue(event->timestamp()); //< Microseconds.

        case Qn::ObjectTrackIdRole:
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
                    q->system()->resourcePool()->getResourceById(sourceId.value<QnUuid>()))
                {
                    return QVariant::fromValue(QnResourceList({resource}));
                }

                if (role == Qn::DisplayedResourceListRole)
                    return {}; //< TODO: #vkutin Replace with <deleted camera> or <deleted server>.
            }

            return {};
        }

        case Qn::ResourceRole: //< Resource for thumbnail preview only.
        {
            if (!hasPreview(event))
                return {};

            return QVariant::fromValue<QnResourcePtr>(
                q->system()->resourcePool()->getResourceById<QnVirtualCameraResource>(
                    details.value(rules::utils::kSourceIdDetailName).value<QnUuid>()));
        }

        case Qn::HelpTopicIdRole:
            return HelpTopic::Id::Empty;

        default:
            handled = false;
            return {};
    }
}

void VmsEventSearchListModel::Private::clearData()
{
    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_liveFetch = {};
}

void VmsEventSearchListModel::Private::truncateToMaximumCount()
{
    if (!q->truncateDataToMaximumCount(m_data, &startTime))
        return;

    if (q->fetchDirection() == FetchDirection::earlier)
        q->setLive(false);
}

void VmsEventSearchListModel::Private::truncateToRelevantTimePeriod()
{
    q->truncateDataToTimePeriod(m_data, &startTime, q->relevantTimePeriod());
}

rest::Handle VmsEventSearchListModel::Private::requestPrefetch(const QnTimePeriod& period)
{
    const auto eventsReceived =
        [this](bool success, rest::Handle requestId, EventLogRecordList&& data)
        {
            if (!requestId || requestId != currentRequest().id)
                return;

            QnTimePeriod actuallyFetched;
            m_prefetch.clear();

            if (success)
            {
                // Ignore events later than current system time.
                truncate(data, currentRequest().direction, qnSyncTime->value());

                m_prefetch = std::move(data);
                if (!m_prefetch.empty())
                {
                    actuallyFetched = QnTimePeriod::fromInterval(
                        m_prefetch.back().timestampMs, m_prefetch.front().timestampMs);
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
bool VmsEventSearchListModel::Private::commitInternal(const QnTimePeriod& periodToCommit,
    Iter prefetchBegin, Iter prefetchEnd, int position, bool handleOverlaps)
{
    const auto begin = std::lower_bound(prefetchBegin, prefetchEnd,
        periodToCommit.endTime(), lowerBoundPredicate);

    auto end = std::upper_bound(prefetchBegin, prefetchEnd,
        periodToCommit.startTime(), upperBoundPredicate);

    if (!m_data.empty() && handleOverlaps)
    {
        const auto& lastData = m_data.front();
        const auto lastTime = startTime(lastData);

        while (end != begin)
        {
            const auto iter = end - 1;
            const auto time = iter->timestampMs;

            if (time > lastTime)
                break;

            end = iter;

            // Trying to deduce if we found exactly the same event which is displayed last.
            // Probably we should remove the last millisecond events from m_data and append all
            // new ones.
            const auto& record =*iter;
            if (time == lastTime && record.eventData == lastData.record().eventData)
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

bool VmsEventSearchListModel::Private::commitPrefetch(const QnTimePeriod& periodToCommit)
{
    const auto clearPrefetch = nx::utils::makeScopeGuard([this]() { m_prefetch.clear(); });

    if (currentRequest().direction == FetchDirection::earlier)
        return commitInternal(periodToCommit, m_prefetch.begin(), m_prefetch.end(), count(), false);

    NX_ASSERT(currentRequest().direction == FetchDirection::later);
    return commitInternal(
        periodToCommit, m_prefetch.rbegin(), m_prefetch.rend(), 0, q->effectiveLiveSupported());
}

void VmsEventSearchListModel::Private::fetchLive()
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
        [this](bool success, rest::Handle requestId, EventLogRecordList&& data)
        {
            const auto scopedClear = nx::utils::makeScopeGuard([this]() { m_liveFetch = {}; });

            if (!success || !q->isLive() || !requestId || requestId != m_liveFetch.id)
                return;

            // Ignore events later than current system time.
            truncate(data, m_liveFetch.direction, qnSyncTime->value());

            if (data.empty())
                return;

            const auto periodToCommit =
                QnTimePeriod::fromInterval(data.back().timestampMs, data.front().timestampMs);

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

rest::Handle VmsEventSearchListModel::Private::getEvents(
    const QnTimePeriod& period,
    GetCallback callback,
    Qt::SortOrder order,
    int limit) const
{
    if (!NX_ASSERT(callback && !q->isFilterDegenerate()))
        return {};

    nx::vms::api::rules::EventLogFilter filter = period.toServerPeriod();

    if (q->cameraSet()->type() != ManagedCameraSet::Type::all)
    {
        filter.eventResourceId = std::vector<QString>();

        for (const auto& camera: q->cameraSet()->cameras())
            filter.eventResourceId->valueOrArray.push_back(camera->getId().toSimpleString());
    }

    filter.eventSubtype = m_selectedSubType;
    filter.limit = limit;
    filter.order = order;
    filter.eventsOnly = true;

    // TODO: #amalov Fix default event types.
    if (!m_selectedEventType.isEmpty())
        filter.eventType = {m_selectedEventType};
    else if (!m_defaultEventTypes.empty())
        filter.eventType = nx::toStdVector(m_defaultEventTypes);

    NX_VERBOSE(q, "Requesting events:\n"
        "    from: %1\n    to: %2\n    type: %3\n    subtype: %4\n    sort: %5\n    limit: %6",
        nx::utils::timestampToDebugString(filter.startTimeMs),
        nx::utils::timestampToDebugString(filter.endTime()),
        filter.eventType.value_or(std::vector<QString>()).valueOrArray,
        filter.eventSubtype,
        filter.order,
        filter.limit);

    const auto internalCallback =
        [this, callback = std::move(callback), guard = QPointer<const Private>(this)](
            bool success, rest::Handle handle, rest::ErrorOrData<EventLogRecordList>&& data)
        {
            if (!guard)
                return;

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
        };

    auto systemContext = q->system();
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

    return api->eventLog(/*serverId*/ {}, filter, std::move(internalCallback), thread());
}

QString VmsEventSearchListModel::Private::description(const QVariantMap& details) const
{
    return details.value(rules::utils::kDetailingDetailName).toStringList()
        .join(common::html::kLineBreak);
}

QPixmap VmsEventSearchListModel::Private::iconPixmap(
    const EventPtr& event,
    const QVariantMap& details) const
{
    using nx::vms::event::Level;
    using nx::vms::rules::Icon;

    const auto icon = details.value(rules::utils::kIconDetailName).value<Icon>();
    const auto level = details.value(rules::utils::kLevelDetailName).value<Level>();
    const auto customIcon = details.value(rules::utils::kCustomIconDetailName).toString();

    QnResourceList devices;
    if (needIconDevices(icon))
    {
        devices = q->system()->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
            rules::utils::getDeviceIds(AggregatedEventPtr::create(event)));
    }

    return eventIcon(icon, level, customIcon, color(details), devices);
}

QColor VmsEventSearchListModel::Private::color(const QVariantMap& details)
{
    if (const auto level = details.value(rules::utils::kLevelDetailName); level.isValid())
    {
        return QnNotificationLevel::notificationTextColor(QnNotificationLevel::convert(
            level.value<nx::vms::event::Level>()));
    }

    return QPalette().light().color();
}

bool VmsEventSearchListModel::Private::hasPreview(const EventPtr& event)
{
    return event->property(rules::utils::kCameraIdFieldName).isValid()
        || event->property(rules::utils::kDeviceIdsFieldName).isValid();
}

} // namespace nx::vms::client::desktop
