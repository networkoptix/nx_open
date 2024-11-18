// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log_model.h"

#include <QtGui/QPalette>

#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/math/math.h>
#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/rules/helpers/help_id.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/resource.h>
#include <nx/vms/time/formatter.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

#include "../utils/event_data.h"
#include "../utils/vms_rules_type_comparator.h"
#include "private/event_model_data.h"

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::vms::rules;

namespace {

int helpTopicIdData(EventLogModel::Column column, const EventLogModelData& data)
{
    switch (column)
    {
        case EventLogModel::EventColumn:
            return eventHelpId(data.eventType());
        case EventLogModel::ActionColumn:
            return actionHelpId(data.actionType());
        default:
            return -1;
    }
}

QString resourceName(const QnResourcePtr& resource)
{
    const auto infoLevel = appContext()->localSettings()->resourceInfoLevel();
    return rules::Strings::resource(resource, infoLevel);
}

QString resourceNames(const QnResourceList& resources)
{
    QStringList names;
    for (const auto& resource: resources)
        names.push_back(resourceName(resource));

    return names.join(", ");
}

QIcon resourceIcon(QnResourceIconCache::Key key)
{
    if ((key & QnResourceIconCache::TypeMask) == QnResourceIconCache::Unknown)
        return {};

    return qnResIconCache->icon(key);
}

QString actionTargetText(SystemContext* context, const EventLogModelData& data)
{
    if (auto destination =
        data.actionDetails(context).value(rules::utils::kDestinationDetailName).toString();
        !destination.isEmpty())
    {
        return destination;
    }

    const auto pool = context->resourcePool();

    if (const auto devices = pool->getResourcesByIds(utils::getDeviceIds(data.action(context)));
        !devices.empty())
    {
        return resourceNames(devices);
    }

    if (const auto servers = pool->getResourcesByIds(utils::getServerIds(data.action(context)));
        !servers.empty())
    {
        return resourceNames(servers);
    }

    return {};
}

QnResourceIconCache::Key actionTargetIcon(SystemContext* context, const EventLogModelData& data)
{
    const auto action = data.action(context);
    auto userCount = action->property("emails").toString().count('@');

    if (userCount > 1)
        return QnResourceIconCache::Users;

    if (const auto userProp = action->property(rules::utils::kUsersFieldName);
        userProp.isValid() && userProp.canConvert<UuidSelection>())
    {
        const auto selection = userProp.value<UuidSelection>();
        if (selection.all)
            return QnResourceIconCache::Users;

        QnUserResourceList users;
        QSet<nx::Uuid> groups;
        nx::vms::common::getUsersAndGroups(context, selection.ids, users, groups);

        if (!groups.empty())
            return QnResourceIconCache::Users;

        userCount += users.size();
    }

    if (userCount > 1)
        return QnResourceIconCache::Users;
    if (userCount == 1)
        return QnResourceIconCache::User;

    const auto pool = context->resourcePool();

    if (const auto devices = pool->getResourcesByIds(utils::getDeviceIds(action));
        !devices.empty())
    {
        if (devices.size() > 1)
            return QnResourceIconCache::Cameras;

        return qnResIconCache->key(devices.first());
    }

    if (const auto servers = pool->getResourcesByIds(utils::getServerIds(action));
        !servers.empty())
    {
        if (servers.size() > 1)
            return QnResourceIconCache::Servers;

        return qnResIconCache->key(servers.first());
    }

    return QnResourceIconCache::Unknown;
}

nx::Uuid eventSourceId(SystemContext* context, const EventLogModelData& data)
{
    return data.details(context).value(utils::kSourceIdDetailName).value<nx::Uuid>();
}

QnResourcePtr eventSource(SystemContext* context, const EventLogModelData& data)
{
    return context->resourcePool()->getResourceById(eventSourceId(context, data));
}

bool hasVideoLink(SystemContext* context, const EventLogModelData& data)
{
    if (!data.record().flags.testFlag(nx::vms::api::rules::EventLogFlag::videoLinkExists))
        return false;

    const auto device = eventSource(context, data).dynamicCast<QnVirtualCameraResource>();

    return device && context->accessController()->hasPermissions(device, Qn::ViewContentPermission);
}

} // namespace

//-------------------------------------------------------------------------------------------------
// EventLogModel::DataIndex

class EventLogModel::DataIndex
{
public:
    using Iterator = EventLogModelDataList::iterator;

public:
    DataIndex(EventLogModel* parent):
        m_parent(parent),
        m_sortCol(DateTimeColumn),
        m_sortOrder(Qt::DescendingOrder),
        m_lexComparator(std::make_unique<VmsRulesTypeComparator>(parent->systemContext()))
    {
    }

    void setSort(Column column, Qt::SortOrder order)
    {
        if (column == m_sortCol)
        {
            m_sortOrder = order;
            return;
        }

        m_sortCol = column;
        m_sortOrder = order;

        updateIndex();
    }

    void setEvents(nx::vms::api::rules::EventLogRecordList&& records)
    {
        m_events.clear();
        m_events.reserve(records.size());
        for(auto&& record: records)
            m_events.emplace_back(std::move(record));

        updateIndex();
    }

    void clear()
    {
        m_events.clear();
    }

    size_t size() const
    {
        return m_events.size();
    }

    bool isValidRow(int row)
    {
        return row >=0 && row < (int) size();
    }

    const EventLogModelData& at(size_t row) const
    {
        return (m_sortOrder == Qt::AscendingOrder) ? *m_records[row] : *m_records[size() - 1 - row];
    }

    bool lessThanTimestamp(Iterator l, Iterator r) const
    {
        return l->record().timestampMs < r->record().timestampMs;
    }

    bool lessThanEventType(Iterator l, Iterator r) const
    {
        const QString lType = l->eventType();
        const QString rType = r->eventType();

        if (lType != rType)
            return m_lexComparator->lexEventLess(lType, rType);

        return lessThanTimestamp(l, r);
    }

    bool lessThanActionType(Iterator l, Iterator r) const
    {
        const QString lType = l->actionType();
        const QString rType = r->actionType();

        if (lType != rType)
            return m_lexComparator->lexActionLess(lType, rType);

        return lessThanTimestamp(l, r);
    }

    bool lessThanLexicographically(Iterator l, Iterator r) const
    {
        int res = l->compareString().compare(r->compareString());
        if (res != 0)
            return res < 0;

        return lessThanTimestamp(l, r);
    }

    void updateIndex()
    {
        m_records.clear();
        m_records.reserve(size());
        for (auto iter = m_events.begin(); iter != m_events.end(); ++iter)
            m_records.push_back(iter);

        typedef bool (DataIndex::*LessFunc)(Iterator, Iterator) const;

        LessFunc lessThan;
        switch (m_sortCol)
        {
            case DateTimeColumn:
                lessThan = &DataIndex::lessThanTimestamp;
                break;
            case EventColumn:
                lessThan = &DataIndex::lessThanEventType;
                break;
            case ActionColumn:
                lessThan = &DataIndex::lessThanActionType;
                break;
            default:
                lessThan = &DataIndex::lessThanLexicographically;
                for (auto record: m_records)
                    record->setCompareString(m_parent->textData(m_sortCol, *record));
                break;
        }

        std::sort(m_records.begin(), m_records.end(),
            [this, lessThan]
            (Iterator d1, Iterator d2)
            {
                return (this->*lessThan)(d1, d2);
            });
    }

private:
    EventLogModel* m_parent;
    Column m_sortCol = {};
    Qt::SortOrder m_sortOrder = {};
    EventLogModelDataList m_events;
    std::vector<Iterator> m_records;
    std::unique_ptr<VmsRulesTypeComparator> m_lexComparator;
};

//-------------------------------------------------------------------------------------------------
// EventLogModel

EventLogModel::EventLogModel(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    m_linkBrush(QPalette().link()),
    m_index(std::make_unique<DataIndex>(this))
{
}

EventLogModel::~EventLogModel()
{
}

void EventLogModel::setEvents(nx::vms::api::rules::EventLogRecordList&& records)
{
    beginResetModel();
    m_index->setEvents(std::move(records));
    endResetModel();
}

QList<EventLogModel::Column> EventLogModel::columns() const
{
    return m_columns;
}

void EventLogModel::setColumns(const QList<Column>& columns)
{
    if (m_columns == columns)
        return;

    beginResetModel();
    for (Column column: columns)
    {
        if (column < 0 || column >= ColumnCount)
        {
            NX_ASSERT(false, lit("Invalid column '%1'.").arg(static_cast<int>(column)));
            return;
        }
    }

    m_columns = columns;
    endResetModel();
}

void EventLogModel::clear()
{
    beginResetModel();
    m_index->clear();
    endResetModel();
}

QModelIndex EventLogModel::index(int row, int column, const QModelIndex& parent) const
{
    return hasIndex(row, column, parent)
        ? createIndex(row, column, nullptr)
        : QModelIndex();
}

QModelIndex EventLogModel::parent(const QModelIndex& /*parent*/) const
{
    return QModelIndex();
}

QVariant EventLogModel::foregroundData(Column column, const EventLogModelData& data) const
{
    if (column == DescriptionColumn && hasVideoLink(systemContext(), data))
        return m_linkBrush;
    return QVariant();
}

QVariant EventLogModel::mouseCursorData(
    Column column, const EventLogModelData& data) const
{
    if (column == DescriptionColumn && hasVideoLink(systemContext(), data))
        return QVariant::fromValue<int>(Qt::PointingHandCursor);
    return QVariant();
}

QnResourcePtr EventLogModel::getResource(Column column, const EventLogModelData& data) const
{
    if (column == EventLogModel::EventCameraColumn)
        return eventSource(systemContext(), data);

    if (column == EventLogModel::ActionCameraColumn)
    {
        if (const auto ids = rules::utils::getResourceIds(data.action(systemContext()));
            !ids.empty())
        {
            return resourcePool()->getResourceById(ids.front());
        }
    }

    return {};
}

QVariant EventLogModel::iconData(Column column, const EventLogModelData& data) const
{
    if (column == ActionCameraColumn)
        return resourceIcon(actionTargetIcon(systemContext(), data));

    if (column == EventCameraColumn)
    {
        if (const auto resource = eventSource(systemContext(), data))
            return qnResIconCache->icon(resource);
    }

    return {};
}

QString EventLogModel::textData(Column column, const EventLogModelData& data) const
{
    const auto& eventDetails = data.details(systemContext());

    switch (column)
    {
        case DateTimeColumn:
        {
            const auto timeWatcher = systemContext()->serverTimeWatcher();
            QDateTime dt = timeWatcher->displayTime(data.record().timestampMs.count());
            return nx::vms::time::toString(dt);
        }
        case EventColumn:
            return eventTitle(eventDetails);

        case EventCameraColumn:
        {
            QString result = eventDetails.value(rules::utils::kSourceNameDetailName).toString();
            if (result.isEmpty())
                result = resourceName(getResource(EventCameraColumn, data));
            return result;
        }
        case ActionColumn:
            return rules::Strings::actionName(systemContext(), data.actionType());

        case ActionCameraColumn:
            return actionTargetText(systemContext(), data);

        case DescriptionColumn:
            return description(data);

        default:
            return QString();
    }
}

QString EventLogModel::htmlData(Column column, const EventLogModelData& data) const
{
    QString text = textData(column, data);

    if (const auto url = motionUrl(column, data); !url.isEmpty())
        return nx::vms::common::html::customLink(text, url);

    return text;
}

QString EventLogModel::tooltip(Column column, const EventLogModelData& data) const
{
    if (column != DescriptionColumn)
        return QString();

    return textData(column, data);
}

QString EventLogModel::description(const EventLogModelData& data) const
{
    auto result =
        data.actionDetails(systemContext()).value(rules::utils::kDescriptionDetailName).toString();

    if (result.isEmpty())
        result = nx::vms::rules::Strings::eventDetails(data.details(systemContext())).join('\n');

    if (result.isEmpty() && hasVideoLink(systemContext(), data))
    {
        if (const auto device = eventSource(systemContext(), data)
            .dynamicCast<QnVirtualCameraResource>())
        {
            if (systemContext()->accessController()->hasPermissions(
                device, Qn::ViewFootagePermission))
            {
                result = tr("Open event video");
            }
            else
            {
                result = QnDeviceDependentStrings::getNameFromSet(
                    resourcePool(),
                    {tr("Open event device"), tr("Open event camera")},
                    device);
            }
        }
    }

    return result;
}

void EventLogModel::sort(int column, Qt::SortOrder order)
{
    beginResetModel();
    m_index->setSort(static_cast<Column>(column), order);
    endResetModel();
}

QString EventLogModel::motionUrl(Column column, const EventLogModelData& data) const
{
    if (column != DescriptionColumn || !hasVideoLink(systemContext(), data))
        return {};

    const auto device = getResource(EventCameraColumn, data)
        .dynamicCast<QnVirtualCameraResource>();
    if (!device)
        return {};

    nx::network::SocketAddress connectionAddress;
    if (auto connection = this->connection(); NX_ASSERT(connection))
        connectionAddress = connection->address();

    return rules::Strings::urlForCamera(
        device,
        data.event(systemContext())->timestamp(),
        rules::Strings::publicIp, //< Not used on the client side anyway.
        connectionAddress);
}

QnResourceList EventLogModel::resourcesForPlayback(const QModelIndex &index) const
{
    QnResourceList result;
    if (index.column() != DescriptionColumn || !m_index->isValidRow(index.row()))
        return {};

    const auto& data = m_index->at(index.row());
    if (!hasVideoLink(systemContext(), data))
        return {};

    return resourcePool()->getResourcesByIds(rules::utils::getDeviceIds(
        AggregatedEventPtr::create(data.event(systemContext()))));
}

int EventLogModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_columns.size();
    return 0;
}

int EventLogModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_index->size();
    return 0;
}

QVariant EventLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < m_columns.size())
    {
        switch (m_columns[section])
        {
            case DateTimeColumn:        return tr("Date/Time");
            case EventColumn:           return tr("Event");
            case EventCameraColumn:     return tr("Source");
            case ActionColumn:          return tr("Action");
            case ActionCameraColumn:    return tr("Target");
            case DescriptionColumn:     return tr("Description");
            default:
                break;
        }
    }
    return base_type::headerData(section, orientation, role);
}

QVariant EventLogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this
        || !hasIndex(index.row(), index.column(), index.parent()))
    {
        return QVariant();
    }

    Column column = m_columns[index.column()];

    if (index.row() < 0 || index.row() >= (int) m_index->size())
        return QVariant();

    const EventLogModelData& data = m_index->at(index.row());

    switch (role)
    {
        case Qt::ToolTipRole:
            return tooltip(column, data);

        case Qt::DisplayRole:
            return textData(column, data);

        case Qt::DecorationRole:
            return iconData(column, data);

        case Qt::ForegroundRole:
            return foregroundData(column, data);

        case Qn::ItemMouseCursorRole:
            return mouseCursorData(column, data);

        case core::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(getResource(column, data));

        case Qn::DisplayHtmlRole:
            return htmlData(column, data);

        case Qn::HelpTopicIdRole:
            return helpTopicIdData(column, data);

        default:
            break;
    }

    return QVariant();
}

QString EventLogModel::eventType(int row) const
{
    return row >= 0 && row < (int) m_index->size()
        ? m_index->at(row).eventType()
        : QString();
}

vms::api::analytics::EventTypeId EventLogModel::analyticsEventType(int row) const
{
    if (!m_index->isValidRow(row))
        return {};

    return m_index->at(row).event(systemContext())
        ->property(rules::utils::kAnalyticsEventTypeDetailName).toString();
}

QnResourcePtr EventLogModel::eventResource(int row) const
{
    if (!m_index->isValidRow(row))
        return {};

    return getResource(EventCameraColumn, m_index->at(row));
}

std::chrono::milliseconds EventLogModel::eventTimestamp(int row) const
{
    if (!m_index->isValidRow(row))
        return milliseconds::zero();

    return duration_cast<milliseconds>(m_index->at(row).event(systemContext())->timestamp());
}

} // namespace nx::vms::client::desktop
