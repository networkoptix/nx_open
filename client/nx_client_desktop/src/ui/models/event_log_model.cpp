#include "event_log_model.h"

#include <cassert>

#include <QtGui/QPalette>

// TODO: #vkutin Think of a proper location and namespace
#include <business/business_types_comparator.h>

#include <nx/vms/event/events/reasoned_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/event/analytics_helper.h>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <client/client_settings.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/help/business_help.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>

using namespace nx;

using eventIterator = vms::event::ActionDataList::iterator;

// -------------------------------------------------------------------------- //
// QnEventLogModel::DataIndex
// -------------------------------------------------------------------------- //
class QnEventLogModel::DataIndex
{
public:
    DataIndex(QnEventLogModel* parent):
        m_parent(parent),
        m_sortCol(DateTimeColumn),
        m_sortOrder(Qt::DescendingOrder),
        m_lexComparator(new QnBusinessTypesComparator(false))
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
            return m_lexComparator->lexicographicalLessThan(d1->eventParams.eventType, d2->eventParams.eventType);
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

// -------------------------------------------------------------------------- //
// QnEventLogModel
// -------------------------------------------------------------------------- //
QnEventLogModel::QnEventLogModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_columns(),
    m_linkBrush(QPalette().link()),
    m_index(new DataIndex(this)),
    m_stringsHelper(new vms::event::StringsHelper(commonModule()))
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
    vms::event::EventType eventType = action.eventParams.eventType;
    if (action.hasFlags(vms::event::ActionData::VideoLinkExists)
        && hasAccessToCamera(action.eventParams.eventResourceId))
    {
        return true;
    }

    if (eventType >= vms::event::userDefinedEvent)
    {
        for (const QnUuid& id: action.eventParams.metadata.cameraRefs)
        {
            if (resourcePool()->getResourceById(id) && hasAccessToCamera(id))
                return true;
        }
    }
    return false;
}

QVariant QnEventLogModel::foregroundData(Column column, const vms::event::ActionData& action) const
{
    if (column == DescriptionColumn && hasVideoLink(action))
        return m_linkBrush;
    return QVariant();
}

QVariant QnEventLogModel::mouseCursorData(Column column, const vms::event::ActionData& action) const
{
    if (column == DescriptionColumn && hasVideoLink(action))
        return QVariant::fromValue<int>(Qt::PointingHandCursor);
    return QVariant();
}

QnResourcePtr QnEventLogModel::getResource(Column column, const vms::event::ActionData& action) const
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
    QList<QnUuid> roles;
    userRolesManager()->usersAndRoles(ids, users, roles);

    const int numDeleted = int(ids.size()) - (users.size() + roles.size());
    NX_EXPECT(numDeleted >= 0);
    if (numDeleted <= 0)
        return m_stringsHelper->actionSubjects(users, roles);

    const QString removedSubjectsText = tr("%n Removed subjects", "", numDeleted);
    return roles.empty() && users.empty()
        ? removedSubjectsText
        : m_stringsHelper->actionSubjects(users, roles, false) + lit(", ") + removedSubjectsText;
}

QString QnEventLogModel::getSubjectNameById(const QnUuid& id) const
{
    static const auto kRemovedUserName = L'<' + tr("Subject removed") + L'>';

    QnUserResourceList users;
    QList<QnUuid> roles;
    userRolesManager()->usersAndRoles(
        QVector<QnUuid>{id}, users, roles);

    return users.empty() && roles.empty()
        ? kRemovedUserName
        : m_stringsHelper->actionSubjects(users, roles, false);
}

QVariant QnEventLogModel::iconData(Column column, const vms::event::ActionData& action)
{
    QnUuid resId;
    switch (column)
    {
        case EventCameraColumn:
            resId = action.eventParams.eventResourceId;
            break;
        case ActionCameraColumn:
        {
            vms::event::ActionType actionType = action.actionType;
            if (actionType == vms::event::sendMailAction)
            {
                if (!action.actionParams.emailAddress.isEmpty())
                {
                    if (action.actionParams.emailAddress.count(L'@') > 1)
                        return qnResIconCache->icon(QnResourceIconCache::Users);
                    else
                        return qnResIconCache->icon(QnResourceIconCache::User);
                }
                else
                {
                    return QVariant();
                }
            }
            else if (actionType == vms::event::showPopupAction
                  || actionType == vms::event::showOnAlarmLayoutAction)
            {
                QnUserResourceList users;
                QList<QnUuid> roles;
                qnClientCoreModule->commonModule()->userRolesManager()->usersAndRoles(
                    action.actionParams.additionalResources, users, roles);
                const bool multiple = action.actionParams.additionalResources.empty()
                    || action.actionParams.additionalResources.size() > 1
                    || users.size() > 1
                    || !roles.empty();
                return qnResIconCache->icon(multiple
                    ? QnResourceIconCache::Users
                    : QnResourceIconCache::User);
            }
        }
        resId = action.actionParams.actionResourceId;
        default:
            break;
    }

    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    return qnResIconCache->icon(resourcePool->getResourceById(resId));
}

QString QnEventLogModel::getResourceNameString(const QnUuid& id) const
{
    return QnResourceDisplayInfo(resourcePool()->getResourceById(id))
        .toString(qnSettings->extraInfoInTree());
}

QString QnEventLogModel::textData(Column column, const vms::event::ActionData& action) const
{
    switch (column)
    {
        case DateTimeColumn:
        {
            qint64 timestampMs = action.eventParams.eventTimestampUsec / 1000;

            QDateTime dt = context()->instance<QnWorkbenchServerTimeWatcher>()->displayTime(timestampMs);
            return dt.toString(Qt::DefaultLocaleShortDate);
        }
        case EventColumn:
        {
            if (action.eventParams.eventType == nx::vms::event::EventType::analyticsSdkEvent)
            {
                const auto camera = resourcePool()->getResourceById(
                    action.eventParams.eventResourceId).
                    dynamicCast<QnVirtualCameraResource>();

                QString eventName = camera
                    ? m_analyticsHelper->eventName(camera,
                        action.eventParams.analyticsEventId(),
                        qnRuntime->locale())
                    : QString();

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
                case vms::event::sendMailAction:
                    return action.actionParams.emailAddress;

                case vms::event::showPopupAction:
                case vms::event::showOnAlarmLayoutAction:
                    return action.actionParams.allUsers
                        ? tr("All users")
                        : getSubjectsText(action.actionParams.additionalResources);

                case vms::event::execHttpRequestAction:
                    return QUrl(action.actionParams.url).toString(QUrl::RemoveUserInfo);

                default:
                    return getResourceNameString(action.actionParams.actionResourceId);
            }
        }
        case DescriptionColumn:
        {
            switch (action.actionType)
            {
                case vms::event::showOnAlarmLayoutAction:
                    return getResourceNameString(action.actionParams.actionResourceId);

                // TODO: #future Rework code to use bookmark.description field for bookmark action.
                case vms::event::acknowledgeAction:
                    /*fallthrough*/
                case vms::event::showTextOverlayAction:
                {
                    const auto text = action.actionParams.text.trimmed();
                    if (!text.isEmpty())
                        return text;
                }
                default:
                    break;
            }

            vms::event::EventType eventType = action.eventParams.eventType;
            QString result;

            if (eventType == vms::event::cameraMotionEvent)
            {
                if (hasVideoLink(action))
                {
                    const auto cameraId = action.eventParams.eventResourceId;
                    result = (hasAccessToArchive(cameraId) ? tr("Motion video") : tr("Open camera"));
                }
            }
            else
            {
                result = m_stringsHelper->eventDetails(action.eventParams).join(L'\n');
            }

            if (!vms::event::hasToggleState(eventType))
            {
                int count = action.aggregationCount;
                if (count > 1)
                {
                    const auto countString = tr("%1 (%n times)"
                        , "%1 is description of event. Will be replaced in runtime", count);
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
    static const auto kDelimiter = L'\n';
    static const int kMaxResourcesCount = 20;

    if (column == ActionCameraColumn && action.actionType == vms::event::showOnAlarmLayoutAction)
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
    // TODO: #GDM #3.1 following block must be moved to ::eventDetails method. Problem is to display
    // too long text in the column (::textData() method).
    if (action.eventParams.eventType == vms::event::licenseIssueEvent
        && action.eventParams.reasonCode == vms::event::EventReason::licenseRemoved)
    {
        QStringList disabledCameras;
        for (const QString& stringId: action.eventParams.description.split(L';'))
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
    return resourceAccessProvider()->hasAccess(context()->user(), camera);
}

bool QnEventLogModel::hasAccessToArchive(const QnUuid& cameraId) const
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId);
    NX_ASSERT(camera, "Resource is not a camera");
    if (!camera)
        return false;

    return resourceAccessProvider()->hasAccess(context()->user(), camera)
        && accessController()->hasGlobalPermission(Qn::GlobalViewArchivePermission);
}

int QnEventLogModel::helpTopicIdData(Column column, const vms::event::ActionData& action)
{
    switch (column)
    {
        case EventColumn:
            return QnBusiness::eventHelpId(action.eventParams.eventType);
        case ActionColumn:
            return QnBusiness::actionHelpId(action.actionType);
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

    return m_stringsHelper->urlForCamera(action.eventParams.eventResourceId, action.eventParams.eventTimestampUsec, true);
}

QnResourceList QnEventLogModel::resourcesForPlayback(const QModelIndex &index) const
{
    QnResourceList result;
    if (!index.isValid() || index.column() != DescriptionColumn)
        return QnResourceList();
    const vms::event::ActionData& action = m_index->at(index.row());
    if (action.hasFlags(vms::event::ActionData::VideoLinkExists))
    {
        QnResourcePtr resource = resourcePool()->getResourceById(action.eventParams.eventResourceId);
        if (resource)
            result << resource;
    }
    result << resourcePool()->getResources(action.eventParams.metadata.cameraRefs);
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
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

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
            else
                return lit("<a href=\"%1\">%2</a>").arg(url, text);
        }

        case Qn::HelpTopicIdRole:
            return helpTopicIdData(column, action);

        default:
            break;
    }

    return QVariant();
}

vms::event::EventType QnEventLogModel::eventType(int row) const
{
    if (row >= 0)
    {
        const vms::event::ActionData& action = m_index->at(row);
        return action.eventParams.eventType;
    }
    return vms::event::undefinedEvent;
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
        const bool accessDenied = ((action.eventParams.eventType == vms::event::cameraMotionEvent)
            && !hasAccessToArchive(action.eventParams.eventResourceId));

        return (accessDenied ? AV_NOPTS_VALUE : action.eventParams.eventTimestampUsec);
    }
    return AV_NOPTS_VALUE;
}
