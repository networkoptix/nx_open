// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log_model.h"

#include <cassert>

#include <QtGui/QPalette>
#include <QtGui/QTextDocument>

#include <api/helpers/camera_id_helper.h>
#include <business/business_types_comparator.h> //< TODO: #vkutin Think of a proper location.
#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/rules_help.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/events/reasoned_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/time/formatter.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

using namespace nx;
using namespace nx::vms::client::desktop;

using eventIterator = vms::event::ActionDataList::iterator;
using nx::vms::api::EventType;
using nx::vms::api::ActionType;

//-------------------------------------------------------------------------------------------------
// QnEventLogModel::DataIndex

class QnEventLogModel::DataIndex
{
public:
    DataIndex(QnEventLogModel* parent):
        m_parent(parent),
        m_sortCol(DateTimeColumn),
        m_sortOrder(Qt::DescendingOrder),
        m_lexComparator(new QnBusinessTypesComparator())
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

    void setEvents(vms::event::ActionDataList events)
    {
        m_events = std::move(events);
        updateIndex();
    }

    const vms::event::ActionDataList& events() const
    {
        return m_events;
    }

    void clear()
    {
        m_events.clear();
    }

    inline int size() const
    {
        return (int) m_events.size();
    }

    inline vms::event::ActionData& at(int row)
    {
        return m_sortOrder == Qt::AscendingOrder ? *m_records[row] : *m_records[size() - 1 - row];
    }

    // comparators

    bool lessThanTimestamp(eventIterator d1, eventIterator d2) const
    {
        return d1->eventParams.eventTimestampUsec < d2->eventParams.eventTimestampUsec;
    }

    bool lessThanEventType(eventIterator d1, eventIterator d2) const
    {
        if (d1->eventParams.eventType != d2->eventParams.eventType)
        {
            return m_lexComparator->lexicographicalLessThan(
                d1->eventParams.eventType, d2->eventParams.eventType);
        }
        return lessThanTimestamp(d1, d2);
    }

    bool lessThanActionType(eventIterator d1, eventIterator d2) const
    {
        if (d1->actionType != d2->actionType)
            return m_lexComparator->lexicographicalLessThan(d1->actionType, d2->actionType);
        return lessThanTimestamp(d1, d2);
    }

    bool lessThanLexicographically(eventIterator d1, eventIterator d2) const
    {
        int rez = d1->compareString.compare(d2->compareString);
        if (rez != 0)
            return rez < 0;
        return lessThanTimestamp(d1, d2);
    }

    void updateIndex()
    {
        m_records.clear();
        m_records.reserve(size());
        for (auto iter = m_events.begin(); iter != m_events.end(); ++iter)
            m_records.push_back(iter);

        typedef bool (DataIndex::*LessFunc)(eventIterator, eventIterator) const;

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
                    record->compareString = m_parent->textData(m_sortCol, *record);
                break;
        }

        std::sort(m_records.begin(), m_records.end(),
            [this, lessThan]
            (eventIterator d1, eventIterator d2)
            {
                return (this->*lessThan)(d1, d2);
            });
    }

private:
    QnEventLogModel* m_parent;
    Column m_sortCol;
    Qt::SortOrder m_sortOrder;
    vms::event::ActionDataList m_events;
    std::vector<eventIterator> m_records;
    QScopedPointer<QnBusinessTypesComparator> m_lexComparator;
};

//-------------------------------------------------------------------------------------------------
// QnEventLogModel

QnEventLogModel::QnEventLogModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_linkBrush(QPalette().link()),
    m_index(new DataIndex(this)),
    m_stringsHelper(new vms::event::StringsHelper(systemContext()))
{
}

QnEventLogModel::~QnEventLogModel()
{
}

void QnEventLogModel::setEvents(vms::event::ActionDataList events)
{
    beginResetModel();
    m_index->setEvents(std::move(events));
    endResetModel();
}

QList<QnEventLogModel::Column> QnEventLogModel::columns() const
{
    return m_columns;
}

void QnEventLogModel::setColumns(const QList<Column>& columns)
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

void QnEventLogModel::clear()
{
    beginResetModel();
    m_index->clear();
    endResetModel();
}

QModelIndex QnEventLogModel::index(int row, int column, const QModelIndex& parent) const
{
    return hasIndex(row, column, parent)
        ? createIndex(row, column, (void*)0)
        : QModelIndex();
}

QModelIndex QnEventLogModel::parent(const QModelIndex& /*parent*/) const
{
    return QModelIndex();
}

bool QnEventLogModel::hasVideoLink(const vms::event::ActionData& action) const
{
    vms::api::EventType eventType = action.eventParams.eventType;
    if (action.hasFlags(vms::event::ActionData::VideoLinkExists)
        && hasAccessToCamera(action.eventParams.eventResourceId))
    {
        return true;
    }

    if (eventType >= EventType::userDefinedEvent)
    {
        for (const auto& flexibleId: action.eventParams.metadata.cameraRefs)
        {
            auto id = camera_id_helper::flexibleIdToId(resourcePool(), flexibleId);
            if (!id.isNull() && hasAccessToCamera(id))
                return true;
        }
    }
    return false;
}

QVariant QnEventLogModel::foregroundData(
    Column column, const vms::event::ActionData& action) const
{
    if (column == DescriptionColumn && hasVideoLink(action))
        return m_linkBrush;
    return QVariant();
}

QVariant QnEventLogModel::mouseCursorData(
    Column column, const vms::event::ActionData& action) const
{
    if (column == DescriptionColumn && hasVideoLink(action))
        return QVariant::fromValue<int>(Qt::PointingHandCursor);
    return QVariant();
}

QnResourcePtr QnEventLogModel::getResource(
    Column column, const vms::event::ActionData& action) const
{
    switch (column)
    {
        case EventCameraColumn:
            return resourcePool()->getResourceById(action.eventParams.eventResourceId);
        case ActionCameraColumn:
            return resourcePool()->getResourceById(action.actionParams.actionResourceId);
        default:
            break;
    }
    return QnResourcePtr();
}

QString QnEventLogModel::getSubjectsText(const std::vector<QnUuid>& ids) const
{
    QnUserResourceList users;
    nx::vms::api::UserGroupDataList groups;
    nx::vms::common::getUsersAndGroups(systemContext(), ids, users, groups);

    const int numDeleted = int(ids.size()) - (users.size() + groups.size());
    NX_ASSERT(numDeleted >= 0);
    if (numDeleted <= 0)
        return m_stringsHelper->actionSubjects(users, groups);

    const QString removedSubjectsText = tr("%n Removed subjects", "", numDeleted);
    return groups.empty() && users.empty()
        ? removedSubjectsText
        : m_stringsHelper->actionSubjects(users, groups, false) + lit(", ") + removedSubjectsText;
}

QString QnEventLogModel::getSubjectNameById(const QnUuid& id) const
{
    static const auto kRemovedUserName = '<' + tr("Subject removed") + '>';

    QnUserResourceList users;
    nx::vms::api::UserGroupDataList groups;
    nx::vms::common::getUsersAndGroups(
        systemContext(), QVector<QnUuid>{id}, users, groups);

    return users.empty() && groups.empty()
        ? kRemovedUserName
        : m_stringsHelper->actionSubjects(users, groups, false);
}

QVariant QnEventLogModel::iconData(Column column, const vms::event::ActionData& action) const
{
    QnUuid resId;
    switch (column)
    {
        case EventCameraColumn:
            resId = action.eventParams.eventResourceId;
            break;
        case ActionCameraColumn:
        {
            vms::api::ActionType actionType = action.actionType;
            if (actionType == ActionType::sendMailAction)
            {
                if (!action.actionParams.emailAddress.isEmpty())
                {
                    if (action.actionParams.emailAddress.count('@') > 1)
                        return qnResIconCache->icon(QnResourceIconCache::Users);
                    else
                        return qnResIconCache->icon(QnResourceIconCache::User);
                }
                else
                {
                    return QVariant();
                }
            }
            else if (actionType == ActionType::showPopupAction
                  || actionType == ActionType::pushNotificationAction
                  || actionType == ActionType::showOnAlarmLayoutAction)
            {
                QnUserResourceList users;
                QList<QnUuid> groups;
                nx::vms::common::getUsersAndGroups(systemContext(),
                    action.actionParams.additionalResources, users, groups);
                const bool multiple = action.actionParams.additionalResources.empty()
                    || action.actionParams.additionalResources.size() > 1
                    || users.size() > 1
                    || !groups.empty();
                return qnResIconCache->icon(multiple
                    ? QnResourceIconCache::Users
                    : QnResourceIconCache::User);
            }
        }
        resId = action.actionParams.actionResourceId;
        default:
            break;
    }

    auto resourcePool = qnClientCoreModule->resourcePool();
    return qnResIconCache->icon(resourcePool->getResourceById(resId));
}

QString QnEventLogModel::getResourceNameString(const QnUuid& id) const
{
    return QnResourceDisplayInfo(resourcePool()->getResourceById(id)).toString(
        appContext()->localSettings()->resourceInfoLevel());
}

bool isAggregationDoneByCameras(const vms::event::ActionData& action)
{
    using namespace nx::vms::api;
    auto eventType = action.eventParams.eventType;
    if ((eventType == cameraDisconnectEvent || eventType == networkIssueEvent)
        && action.actionType == sendMailAction)
    {
        return true;
    }
    return false;
}

QString QnEventLogModel::textData(Column column, const vms::event::ActionData& action) const
{
    using namespace nx::analytics::taxonomy;

    switch (column)
    {
        case DateTimeColumn:
        {
            qint64 timestampMs = action.eventParams.eventTimestampUsec / 1000;

            // TODO: #sivanov Actualize used system context.
            const auto timeWatcher = appContext()->currentSystemContext()->serverTimeWatcher();
            QDateTime dt = timeWatcher->displayTime(timestampMs);
            return nx::vms::time::toString(dt);
        }
        case EventColumn:
        {
            if (action.eventParams.eventType == nx::vms::api::EventType::analyticsSdkEvent)
            {
                const std::shared_ptr<AbstractState> taxonomyState =
                    systemContext()->analyticsTaxonomyState();

                if (!taxonomyState)
                    m_stringsHelper->eventName(action.eventParams.eventType);

                const AbstractEventType* eventType = taxonomyState->eventTypeById(
                    action.eventParams.getAnalyticsEventTypeId());
                QString eventName = eventType ? eventType->name() : QString();
                if (!eventName.isEmpty())
                    return eventName;
            }

            return m_stringsHelper->eventName(action.eventParams.eventType);
        }

        case EventCameraColumn:
        {
            QString result = getResourceNameString(action.eventParams.eventResourceId);
            if (result.isEmpty())
                result = action.eventParams.resourceName;
            return result;
        }
        case ActionColumn:
            return m_stringsHelper->actionName(action.actionType);

        case ActionCameraColumn:
        {
            switch (action.actionType)
            {
                case ActionType::sendMailAction:
                    return action.actionParams.emailAddress;

                case ActionType::showPopupAction:
                case ActionType::pushNotificationAction:
                case ActionType::showOnAlarmLayoutAction:
                    return action.actionParams.allUsers
                        ? tr("All users")
                        : getSubjectsText(action.actionParams.additionalResources);

                case ActionType::execHttpRequestAction:
                    return QUrl(action.actionParams.url).toString(QUrl::RemoveUserInfo);

                default:
                    return getResourceNameString(action.actionParams.actionResourceId);
            }
        }
        case DescriptionColumn:
        {
            switch (action.actionType)
            {
                case ActionType::showOnAlarmLayoutAction:
                    return getResourceNameString(action.actionParams.actionResourceId);

                // TODO: Future: Rework code to use bookmark.description field for bookmark action.
                case ActionType::acknowledgeAction:
                    /*fallthrough*/
                case ActionType::showTextOverlayAction:
                {
                    const auto text = action.actionParams.text.trimmed();
                    if (!text.isEmpty())
                        return nx::vms::common::html::toPlainText(text);
                }
                default:
                    break;
            }

            vms::api::EventType eventType = action.eventParams.eventType;
            QString result;

            if (eventType ==EventType::cameraMotionEvent)
            {
                if (hasVideoLink(action))
                {
                    const auto cameraId = action.eventParams.eventResourceId;
                    result = hasAccessToArchive(cameraId) ? tr("Motion video") : tr("Open camera");
                }
            }
            else
            {
                result = nx::vms::common::html::toPlainText(m_stringsHelper->eventDetails(
                    action.eventParams,
                    nx::vms::event::AttrSerializePolicy::singleLine
                ).join('\n'));
            }

            if (!vms::event::hasToggleState(eventType, action.eventParams, systemContext()))
            {
                int count = action.aggregationCount;

                // For Ldap Sync Issue events we print detailed (aggregated by subtypes) data that
                // already contains the number of event occurances in the event description. For
                // all other event types we add aggregation count to the result, if necessary.
                if (count > 1 && eventType != EventType::ldapSyncIssueEvent)
                {
                    QString countString;
                    if (isAggregationDoneByCameras(action))
                    {
                        countString = tr("%1 (%n cameras)",
                            "%1 is description of event. Will be replaced in runtime", count);
                    }
                    else
                    {
                        countString = tr("%1 (%n times)",
                            "%1 is description of event. Will be replaced in runtime", count);
                    }
                    return countString.arg(result);
                }
            }
            return result;
        }
        default:
            return QString();
    }
}

QString QnEventLogModel::tooltip(Column column, const vms::event::ActionData& action) const
{
    static const auto kDelimiter = QChar('\n');
    static const int kMaxResourcesCount = 20;

    if (column == ActionCameraColumn && action.actionType == ActionType::showOnAlarmLayoutAction)
    {
        const auto& users = action.actionParams.additionalResources;

        QStringList userNames;
        if (users.empty())
        {
            const auto userResources = resourcePool()->getResources<QnUserResource>();
            for (const auto& resource: userResources)
                userNames.append(resource->getName());
        }
        else
        {
            for (const auto& userId: users)
                userNames.append(getSubjectNameById(userId));
        }

        if (userNames.size() > kMaxResourcesCount)
        {
            const auto moreUsers = userNames.size() - kMaxResourcesCount;

            userNames = userNames.mid(0, kMaxResourcesCount);
            userNames.append(tr("and %n users more...", "", moreUsers));
        }

        return userNames.join(kDelimiter);
    }

    if (column != DescriptionColumn)
        return QString();

    QString result = textData(column, action);
    // TODO: #sivanov #3.1 The following block must be moved to ::eventDetails method. The problem
    // is to display too long text in the column (::textData() method).
    if (action.eventParams.eventType == EventType::licenseIssueEvent
        && action.eventParams.reasonCode == vms::api::EventReason::licenseRemoved)
    {
        QStringList disabledCameras;
        for (const QString& stringId: action.eventParams.description.split(';'))
        {
            QnUuid id = QnUuid::fromStringSafe(stringId);
            NX_ASSERT(!id.isNull());
            if (auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id))
                disabledCameras << QnResourceDisplayInfo(camera).toString(Qn::RI_WithUrl);
        }

        const auto moreCameras = disabledCameras.size() - kMaxResourcesCount;
        if (moreCameras > 0)
            disabledCameras = disabledCameras.mid(0, kMaxResourcesCount);
        NX_ASSERT(!disabledCameras.isEmpty());
        result += kDelimiter + disabledCameras.join(kDelimiter);

        if (moreCameras > 0)
            result += kDelimiter + tr("and %n more...", "", moreCameras);
    }

    return result;
}


bool QnEventLogModel::hasAccessToCamera(const QnUuid& cameraId) const
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId);
    NX_ASSERT(camera, "Resource is not a camera");
    if (!camera)
        return false;

    return systemContext()->accessController()->hasPermissions(camera, Qn::ReadPermission);
}

bool QnEventLogModel::hasAccessToArchive(const QnUuid& cameraId) const
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId);
    NX_ASSERT(camera, "Resource is not a camera");
    if (!camera)
        return false;

    return systemContext()->accessController()->hasPermissions(camera, Qn::ViewFootagePermission);
}

int QnEventLogModel::helpTopicIdData(Column column, const vms::event::ActionData& action)
{
    switch (column)
    {
        case EventColumn:
            return rules::eventHelpId(action.eventParams.eventType);
        case ActionColumn:
            return rules::actionHelpId(action.actionType);
        default:
            return -1;
    }
}

void QnEventLogModel::sort(int column, Qt::SortOrder order)
{
    beginResetModel();
    m_index->setSort(static_cast<Column>(column), order);
    endResetModel();
}

QString QnEventLogModel::motionUrl(Column column, const vms::event::ActionData& action) const
{
    if (column != DescriptionColumn || !action.hasFlags(vms::event::ActionData::VideoLinkExists))
        return QString();

    nx::network::SocketAddress connectionAddress;
    if (auto connection = this->connection(); NX_ASSERT(connection))
        connectionAddress = connection->address();

    return m_stringsHelper->urlForCamera(
        action.eventParams.eventResourceId,
        std::chrono::milliseconds(action.eventParams.eventTimestampUsec / 1000),
        /*usePublicIp*/ true, //< Not used on the client side anyway.
        connectionAddress);
}

QnResourceList QnEventLogModel::resourcesForPlayback(const QModelIndex &index) const
{
    QnResourceList result;
    if (!index.isValid() || index.column() != DescriptionColumn)
        return QnResourceList();
    const vms::event::ActionData& action = m_index->at(index.row());
    if (action.hasFlags(vms::event::ActionData::VideoLinkExists))
    {
        QnResourcePtr resource =
            resourcePool()->getResourceById(action.eventParams.eventResourceId);
        if (resource)
            result << resource;
    }
    QnResourceList extraResources = resourcePool()->getCamerasByFlexibleIds(
        action.eventParams.metadata.cameraRefs);
    result << extraResources;
    return result;
}

int QnEventLogModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_columns.size();
    return 0;
}

int QnEventLogModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_index->size();
    return 0;
}

QVariant QnEventLogModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QVariant QnEventLogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this
        || !hasIndex(index.row(), index.column(), index.parent()))
    {
        return QVariant();
    }

    Column column = m_columns[index.column()];

    if (index.row() < 0 || index.row() >= m_index->size())
        return QVariant();

    const vms::event::ActionData& action = m_index->at(index.row());

    switch (role)
    {
        case Qt::ToolTipRole:
            return tooltip(column, action);

        case Qt::DisplayRole:
            return textData(column, action);

        case Qt::DecorationRole:
            return iconData(column, action);

        case Qt::ForegroundRole:
            return foregroundData(column, action);

        case Qn::ItemMouseCursorRole:
            return mouseCursorData(column, action);

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(getResource(column, action));

        case Qn::DisplayHtmlRole:
        {
            QString text = textData(column, action);
            QString url = motionUrl(column, action);
            if (url.isEmpty())
                return text;

            // Non-standard url is passed.
            return nx::vms::common::html::customLink(text, url);
        }

        case Qn::HelpTopicIdRole:
            return helpTopicIdData(column, action);

        default:
            break;
    }

    return QVariant();
}

vms::api::EventType QnEventLogModel::eventType(int row) const
{
    if (row >= 0)
    {
        const vms::event::ActionData& action = m_index->at(row);
        return action.eventParams.eventType;
    }
    return EventType::undefinedEvent;
}

vms::api::analytics::EventTypeId QnEventLogModel::analyticsEventType(int row) const
{
    if (row >= 0)
    {
        const vms::event::ActionData& action = m_index->at(row);
        if (action.eventParams.eventType != EventType::analyticsSdkEvent)
            return {};

        return action.eventParams.getAnalyticsEventTypeId();
    }

    return {};
}

QnResourcePtr QnEventLogModel::eventResource(int row) const
{
    if (row >= 0)
    {
        const vms::event::ActionData& action = m_index->at(row);
        return resourcePool()->getResourceById(action.eventParams.eventResourceId);
    }
    return QnResourcePtr();
}

qint64 QnEventLogModel::eventTimestamp(int row) const
{
    if (row >= 0)
    {
        const vms::event::ActionData& action = m_index->at(row);
        const bool accessDenied = ((action.eventParams.eventType == EventType::cameraMotionEvent)
            && !hasAccessToArchive(action.eventParams.eventResourceId));

        return (accessDenied ? AV_NOPTS_VALUE : action.eventParams.eventTimestampUsec);
    }
    return AV_NOPTS_VALUE;
}
