#include "event_log_model.h"

#include <cassert>

#include <QtGui/QPalette>

#include <business/events/reasoned_business_event.h>
#include <business/business_strings_helper.h>
#include <business/business_types_comparator.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/help/business_help.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/math/math.h>

namespace
{
    enum { kSingleUser = 1};

    const auto kDelimiter = lit("\n");
}
typedef QnBusinessActionData* QnLightBusinessActionP;

QHash<QnUuid, QnResourcePtr> QnEventLogModel::m_resourcesHash;

// -------------------------------------------------------------------------- //
// QnEventLogModel::DataIndex
// -------------------------------------------------------------------------- //
class QnEventLogModel::DataIndex
{
public:
    DataIndex(QnEventLogModel *parent)
        : m_parent(parent)
        , m_sortCol(DateTimeColumn)
        , m_sortOrder(Qt::DescendingOrder)
        , m_size(0)
        , m_lexComparator(new QnBusinessTypesComparator())
    {

    }


    void setSort(int column, Qt::SortOrder order)
    {
        if ((Column) column == m_sortCol) {
            m_sortOrder = order;
            return;
        }

        m_sortCol = (Column) column;
        m_sortOrder = order;

        updateIndex();
    }

    void setEvents(const QVector<QnBusinessActionDataListPtr>& events)
    {
        m_events = events;
        m_size = 0;
        for (int i = 0; i < events.size(); ++i)
            m_size += (int) events[i]->size();

        updateIndex();
    }

    QVector<QnBusinessActionDataListPtr> events() const
    {
        return m_events;
    }

    void clear()
    {
        m_events.clear();
    }

    inline int size() const
    {
        return m_size;
    }

    inline QnBusinessActionData& at(int row)
    {
        return m_sortOrder == Qt::AscendingOrder ? *m_records[row] : *m_records[m_size-1-row];
    }

    // comparators

    bool lessThanTimestamp(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2) const
    {
        return d1->eventParams.eventTimestampUsec < d2->eventParams.eventTimestampUsec;
    }

    bool lessThanEventType(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2) const
    {
        if (d1->eventParams.eventType != d2->eventParams.eventType)
            return m_lexComparator->lexicographicalLessThan(d1->eventParams.eventType, d2->eventParams.eventType);
        return lessThanTimestamp(d1, d2);
    }

    bool lessThanActionType(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2) const
    {
        if (d1->actionType != d2->actionType)
            return m_lexComparator->lexicographicalLessThan(d1->actionType, d2->actionType);
        return lessThanTimestamp(d1, d2);
    }

    bool lessLexicographically(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2) const
    {
        int rez = d1->compareString.compare(d2->compareString);
        if (rez != 0)
            return rez < 0;
        return lessThanTimestamp(d1, d2);
    }

    void updateIndex()
    {
        m_records.resize(m_size);
        if (m_records.isEmpty())
            return;

        QnLightBusinessActionP* dst = &m_records[0];
        for (int i = 0; i < m_events.size(); ++i)
        {
            QnBusinessActionDataList& data = *m_events[i].data();
            for (uint j = 0; j < data.size(); ++j)
                *dst++ = &data[j];
        }

        // typedef std::function<bool(const DataIndex * /*this*/, const QnLightBusinessActionP &, const QnLightBusinessActionP &)> LessFunc;
        typedef bool (DataIndex::*LessFunc)(const QnLightBusinessActionP &, const QnLightBusinessActionP &) const;

        LessFunc lessThan;
        switch (m_sortCol) {
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
            lessThan = &DataIndex::lessLexicographically;
            for (auto record: m_records)
                record->compareString = m_parent->textData(m_sortCol, *record);
            break;
        }

        std::sort(m_records.begin(), m_records.end(), [this, lessThan](const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2) {
            //return lessThan(this, d1, d2);
            return (this->*lessThan)(d1, d2);
        });
    }

private:
    QnEventLogModel *m_parent;
    Column m_sortCol;
    Qt::SortOrder m_sortOrder;
    QVector<QnBusinessActionDataListPtr> m_events;
    QVector<QnLightBusinessActionP> m_records;
    int m_size;
    QScopedPointer<QnBusinessTypesComparator> m_lexComparator;
};

// -------------------------------------------------------------------------- //
// QnEventLogModel
// -------------------------------------------------------------------------- //
QnEventLogModel::QnEventLogModel(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , m_columns()
    , m_linkBrush(QPalette().link())
    , m_linkFont()
    , m_index(new DataIndex(this))
{
    m_linkFont.setUnderline(true);

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnEventLogModel::at_resource_removed);
}

QnEventLogModel::~QnEventLogModel() {
}

void QnEventLogModel::setEvents(const QVector<QnBusinessActionDataListPtr> &events) {
    beginResetModel();
    m_index->setEvents(events);
    endResetModel();
}

QList<QnEventLogModel::Column> QnEventLogModel::columns() const {
    return m_columns;
}

void QnEventLogModel::setColumns(const QList<Column> &columns) {
    if(m_columns == columns)
        return;

    beginResetModel();
    foreach(Column column, columns) {
        if(column < 0 || column >= ColumnCount) {
            qnWarning("Invalid column '%1'.", static_cast<int>(column));
            return;
        }
    }

    m_columns = columns;
    endResetModel();
}

void QnEventLogModel::clear() {
    beginResetModel();
    m_index->clear();
    endResetModel();
}

QModelIndex QnEventLogModel::index(int row, int column, const QModelIndex &parent) const {
    return hasIndex(row, column, parent)
        ? createIndex(row, column, (void*)0)
        : QModelIndex();
}

QModelIndex QnEventLogModel::parent(const QModelIndex &) const {
    return QModelIndex();
}

bool QnEventLogModel::hasVideoLink(const QnBusinessActionData &action)
{
    QnBusiness::EventType eventType = action.eventParams.eventType;
    if (action.hasFlags(QnBusinessActionData::MotionExists))
    {
        if (eventType == QnBusiness::CameraMotionEvent)
            return true;
    }
    else if (eventType >= QnBusiness::UserDefinedEvent)
    {
        for (const QnUuid& id: action.eventParams.metadata.cameraRefs)
        {
            if (qnResPool->getResourceById(id))
                return true;
        }
    }
    return false;
}

QVariant QnEventLogModel::fontData(const Column& column, const QnBusinessActionData &action) const {
    if (column == DescriptionColumn && hasVideoLink(action))
        return m_linkFont;

    return QVariant();
}

QVariant QnEventLogModel::foregroundData(const Column& column, const QnBusinessActionData &action) const {
    if (column == DescriptionColumn && hasVideoLink(action))
        return m_linkBrush;
    return QVariant();
}

QVariant QnEventLogModel::mouseCursorData(const Column& column, const QnBusinessActionData &action) {
    if (column == DescriptionColumn && hasVideoLink(action))
        return QVariant::fromValue<int>(Qt::PointingHandCursor);
    return QVariant();
}

QnResourcePtr QnEventLogModel::getResource(const Column &column, const QnBusinessActionData &action) const {
    switch(column) {
    case EventCameraColumn:
        return getResourceById(action.eventParams.eventResourceId);
    case ActionCameraColumn:
        return getResourceById(action.actionParams.actionResourceId);
    default:
        break;
    }
    return QnResourcePtr();
}

QnResourcePtr QnEventLogModel::getResourceById(const QnUuid &id) {
    if (id.isNull())
        return QnResourcePtr();

    QnResourcePtr resource = m_resourcesHash.value(id);
    if (resource)
        return resource;

    resource = qnResPool->getResourceById(id);
    if (resource)
        m_resourcesHash.insert(id, resource);

    return resource;
}

QString QnEventLogModel::getUserNameById(const QnUuid &id)
{
    static const auto kRemovedUserName = tr("<User removed>");

    const auto userResource = getResourceById(id).dynamicCast<QnUserResource>();
    return (userResource.isNull() ? kRemovedUserName : userResource->getName());
}


QVariant QnEventLogModel::iconData(const Column& column, const QnBusinessActionData &action) {
    QnUuid resId;
    switch(column) {
    case EventCameraColumn:
        resId = action.eventParams.eventResourceId;
        break;
    case ActionCameraColumn:
        {
            QnBusiness::ActionType actionType = action.actionType;
            if (actionType == QnBusiness::SendMailAction) {
                if (!action.actionParams.emailAddress.isEmpty()) {
                    if (action.actionParams.emailAddress.count(L'@') > 1)
                        return qnResIconCache->icon(QnResourceIconCache::Users);
                    else
                        return qnResIconCache->icon(QnResourceIconCache::User);
                }
                else {
                    return QVariant();
                }
            }
            else if (actionType == QnBusiness::ShowPopupAction) {
                if (action.actionParams.userGroup == QnBusiness::AdminOnly)
                    return qnResIconCache->icon(QnResourceIconCache::User);
                else
                    return qnResIconCache->icon(QnResourceIconCache::Users);
            }
            else if (actionType == QnBusiness::ShowOnAlarmLayoutAction) {
                const auto &users = action.actionParams.additionalResources;
                const bool multipleUsers = (users.empty() || (users.size() > kSingleUser));
                return qnResIconCache->icon(multipleUsers ?
                    QnResourceIconCache::Users : QnResourceIconCache::User);
            }
        }
        resId = action.actionParams.actionResourceId;
    default:
        break;
    }

    return qnResIconCache->icon(getResourceById(resId));
}

QString QnEventLogModel::getResourceNameString(const QnUuid &id)
{
    return QnResourceDisplayInfo(getResourceById(id)).toString(qnSettings->extraInfoInTree());
}

QString QnEventLogModel::getUserGroupString(QnBusiness::UserGroup value) {
    switch (value) {
    case QnBusiness::EveryOne:
        return tr("All Users");
    case QnBusiness::AdminOnly:
        return tr("Administrators Only");
    default:
        return QString();
    }
    return QString();
}

QString QnEventLogModel::textData(const Column& column,const QnBusinessActionData& action) const {
    switch(column) {
    case DateTimeColumn: {
        qint64 timestampMs = action.eventParams.eventTimestampUsec / 1000;

        QDateTime dt = context()->instance<QnWorkbenchServerTimeWatcher>()->displayTime(timestampMs);
        return dt.toString(Qt::SystemLocaleShortDate);
    }
    case EventColumn:
        return QnBusinessStringsHelper::eventName(action.eventParams.eventType);
    case EventCameraColumn:
    {
        QString result = getResourceNameString(action.eventParams.eventResourceId);
        if (result.isEmpty())
            result = action.eventParams.resourceName;
        return result;
    }
    case ActionColumn:
        return QnBusinessStringsHelper::actionName(action.actionType);
    case ActionCameraColumn: {
        QnBusiness::ActionType actionType = action.actionType;
        if (actionType == QnBusiness::SendMailAction)
            return action.actionParams.emailAddress;
        else if (actionType == QnBusiness::ShowPopupAction)
            return getUserGroupString(action.actionParams.userGroup);
        else if (actionType == QnBusiness::ShowOnAlarmLayoutAction) {
            // For ShowOnAlarmLayoutAction action type additionalResources contains users list
            const auto &users = action.actionParams.additionalResources;
            if (users.empty())
                return tr("All users");

            if (users.size() == kSingleUser)
                return getUserNameById(users.front());

            static const auto kUsersTempltate = tr("%1 users");
            return kUsersTempltate.arg(QString::number(users.size()));
        }
        else if (actionType == QnBusiness::ExecHttpRequestAction) {
            return QUrl(action.actionParams.url).toString(QUrl::RemoveUserInfo);
        }
        else
            return getResourceNameString(action.actionParams.actionResourceId);
    }
    case DescriptionColumn:
    {
        switch(action.actionType)
        {
        case QnBusiness::ShowOnAlarmLayoutAction:
            return getResourceNameString(action.actionParams.actionResourceId);

        case QnBusiness::ShowTextOverlayAction:
        {
            const auto text = action.actionParams.text.trimmed();
            if (!text.isEmpty())
                return text;
        }
        default:
            break;
        }

        QnBusiness::EventType eventType = action.eventParams.eventType;
        QString result;

        if (eventType == QnBusiness::CameraMotionEvent)
        {
            if (action.hasFlags(QnBusinessActionData::MotionExists))
                result = tr("Motion video");
        }
        else
        {
            result = QnBusinessStringsHelper::eventDetails(action.eventParams, lit("\n"));
        }

        if (!QnBusiness::hasToggleState(eventType))
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

int QnEventLogModel::helpTopicIdData(const Column& column, const QnBusinessActionData &action) {
    switch(column) {
    case EventColumn:
        return QnBusiness::eventHelpId(action.eventParams.eventType);
    case ActionColumn:
        return QnBusiness::actionHelpId(action.actionType);
    default:
        return -1;
    }
}

void QnEventLogModel::sort(int column, Qt::SortOrder order) {
    beginResetModel();
    m_index->setSort(column, order);
    endResetModel();
}

QString QnEventLogModel::motionUrl(Column column, const QnBusinessActionData& action) {
    if (column != DescriptionColumn || !action.hasFlags(QnBusinessActionData::MotionExists))
        return QString();

    if (action.eventParams.eventType != QnBusiness::CameraMotionEvent)
        return QString();

    return QnBusinessStringsHelper::urlForCamera(action.eventParams.eventResourceId, action.eventParams.eventTimestampUsec, true);
}

QnResourceList QnEventLogModel::resourcesForPlayback(const QModelIndex &index) const
{
    QnResourceList result;
    if (!index.isValid() || index.column() != DescriptionColumn)
        return QnResourceList();
    const QnBusinessActionData &action = m_index->at(index.row());
    if (action.hasFlags(QnBusinessActionData::MotionExists)) {
        QnResourcePtr resource = qnResPool->getResourceById(action.eventParams.eventResourceId);
        if (resource)
            result << resource;
    }
    result << qnResPool->getResources(action.eventParams.metadata.cameraRefs);
    return result;
}

/*
bool QnEventLogModel::hasMotionUrl(const QModelIndex &index) const {

    const QnBusinessActionData &action = m_index->at(index.row());
    if (!action.hasFlags(QnBusinessActionData::MotionExists))
        return false;
    if (action.eventParams.eventResourceId.isNull())
        return false;

    return true;
}
*/

int QnEventLogModel::columnCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_columns.size();
    return 0;
}

int QnEventLogModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_index->size();
    return 0;
}

QVariant QnEventLogModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < m_columns.size()) {
        switch(m_columns[section]) {
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

QVariant QnEventLogModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    if (index.column() < 0 || index.column() >= m_columns.size()) // TODO: #Elric this is probably not needed, hasIndex checks it?
        return QVariant();

    const Column &column = m_columns[index.column()];

    if (index.row() < 0 || index.row() >= m_index->size())
        return QVariant();

    const QnBusinessActionData &action = m_index->at(index.row());

    switch(role) {
    case Qt::ToolTipRole:
    {
        if (index.column() == ActionCameraColumn &&  action.actionType == QnBusiness::ShowOnAlarmLayoutAction)
        {
            enum { kMaxShownUsersCount = 20 };

            const auto users = action.actionParams.additionalResources;

            QStringList userNames;
            if (users.empty())
            {
                const auto userResources = qnResPool->getResources<QnUserResource>();
                for (const auto &resource: userResources)
                    userNames.append(resource->getName());
            }
            else
            {
                for (const auto &userId: users)
                    userNames.append(getUserNameById(userId));
            }

            if (userNames.size() > kMaxShownUsersCount)
            {
                const auto diffCount = (kMaxShownUsersCount - userNames.size());

                userNames = userNames.mid(0, kMaxShownUsersCount);
                userNames.append(tr("and %1 user(s) more...", nullptr, diffCount));
            }

            return userNames.join(kDelimiter);
        }
        else if (index.column() != DescriptionColumn)
        {
            return QVariant();
        }

        // else go to Qt::DisplayRole
    }
    case Qt::DisplayRole:
        return QVariant(textData(column, action));
    case Qt::DecorationRole:
        return iconData(column, action);
    case Qt::FontRole:
        return fontData(column, action);
    case Qt::ForegroundRole:
        return foregroundData(column, action);
    case Qn::ItemMouseCursorRole:
        return mouseCursorData(column, action);
    case Qn::ResourceRole:
        return QVariant::fromValue<QnResourcePtr>(getResource(column, action));
    case Qn::DisplayHtmlRole: {
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

QnBusiness::EventType QnEventLogModel::eventType(int row) const {
    if (row >= 0) {
        const QnBusinessActionData& action = m_index->at(row);
        return action.eventParams.eventType;
    }
    return QnBusiness::UndefinedEvent;
}

QnResourcePtr QnEventLogModel::eventResource(int row) const {
    if (row >= 0) {
        const QnBusinessActionData& action = m_index->at(row);
        return qnResPool->getResourceById(action.eventParams.eventResourceId);
    }
    return QnResourcePtr();
}

qint64 QnEventLogModel::eventTimestamp(int row) const {
    if (row >= 0) {
        const QnBusinessActionData& action = m_index->at(row);
        return action.eventParams.eventTimestampUsec;
    }
    return AV_NOPTS_VALUE;
}

void QnEventLogModel::at_resource_removed(const QnResourcePtr &resource) {
    m_resourcesHash.remove(resource->getId());
}
