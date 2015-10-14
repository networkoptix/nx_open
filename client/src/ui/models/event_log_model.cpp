#include "event_log_model.h"

#include <cassert>

#include <QtGui/QPalette>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include "core/resource/resource.h"
#include "business/events/reasoned_business_event.h"
#include "core/resource_management/resource_pool.h"
#include "ui/style/resource_icon_cache.h"
#include <ui/help/business_help.h>
#include "business/business_strings_helper.h"
#include "client/client_globals.h"
#include <utils/math/math.h>
#include <plugins/resource/server_camera/server_camera.h>
#include "client/client_settings.h"
#include <ui/common/ui_resource_name.h>

typedef QnBusinessActionData* QnLightBusinessActionP;

QHash<QnUuid, QnResourcePtr> QnEventLogModel::m_resourcesHash;

// -------------------------------------------------------------------------- //
// QnEventLogModel::DataIndex
// -------------------------------------------------------------------------- //
class QnEventLogModel::DataIndex
{
public:
    DataIndex(): m_sortCol(DateTimeColumn), m_sortOrder(Qt::DescendingOrder), m_size(0)
    {
        static bool firstCall = true;
        if (firstCall) {
            initStaticData();
            firstCall = false;
        }
    }

    void initStaticData()
    {
        // event types to lex order
        QMap<QString, int> events;
        for (int i = 0; i < 256; ++i) {
            events.insert(QnBusinessStringsHelper::eventName(QnBusiness::EventType(i)), i);
            m_eventTypeToLexOrder[i] = 255; // put undefined events to the end of the list
        }
        int count = 0;
        for(QMap<QString, int>::const_iterator itr = events.begin(); itr != events.end(); ++itr)
            m_eventTypeToLexOrder[itr.value()] = count++;

        // action types to lex order
        QMap<QString, int> actions;
        for (int i = 0; i < 256; ++i) {
            actions.insert(QnBusinessStringsHelper::actionName(QnBusiness::ActionType(i)), i);
            m_actionTypeToLexOrder[i] = 255; // put undefined actions to the end of the list
        }
        count = 0;
        for(QMap<QString, int>::const_iterator itr = actions.begin(); itr != actions.end(); ++itr)
            m_actionTypeToLexOrder[itr.value()] = count++;
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

    /*
     * Reorder event types to lexicographical order (for sorting)
     */
    static int toLexEventType(QnBusiness::EventType eventType)
    {
        return m_eventTypeToLexOrder[((int) eventType) & 0xff];
    }

    /*
     * Reorder actions types to lexicographical order (for sorting)
     */
    static int toLexActionType(QnBusiness::ActionType actionType)
    {
        return m_actionTypeToLexOrder[((int) actionType) & 0xff];
    }

    inline QnBusinessActionData& at(int row)
    {
        return m_sortOrder == Qt::AscendingOrder ? *m_records[row] : *m_records[m_size-1-row];
    }

    // comparators

    typedef bool (*LessFunc)(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2);

    static bool lessThanTimestamp(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2)
    {
        return d1->eventParams.eventTimestampUsec < d2->eventParams.eventTimestampUsec;
    }

    static bool lessThanEventType(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2)
    {
        int r1 = QnEventLogModel::DataIndex::toLexEventType(d1->eventParams.eventType);
        int r2 = QnEventLogModel::DataIndex::toLexEventType(d2->eventParams.eventType);
        if (r1 < r2)
            return true;
        else if (r1 > r2)
            return false;
        else
            return lessThanTimestamp(d1, d2);
    }

    static bool lessThanActionType(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2)
    {
        int r1 = QnEventLogModel::DataIndex::toLexActionType(d1->actionType);
        int r2 = QnEventLogModel::DataIndex::toLexActionType(d2->actionType);
        if (r1 < r2)
            return true;
        else if (r1 > r2)
            return false;
        else
            return lessThanTimestamp(d1, d2);
    }

    static bool lessLexographic(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2)
    {
        int rez = d1->compareString.compare(d2->compareString);
        if (rez < 0)
            return true;
        else if (rez > 0)
            return false;
        else
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

        LessFunc lessThan;
        switch(m_sortCol) {
        case DateTimeColumn:
            lessThan = &lessThanTimestamp;
            break;
        case EventColumn:
            lessThan = &lessThanEventType;
            break;
        case ActionColumn:
            lessThan = &lessThanActionType;
            break;
        default:
            lessThan = &lessLexographic;
            for (int i = 0; i < m_records.size(); ++i)
                m_records[i]->compareString = QnEventLogModel::textData(m_sortCol, *m_records[i]);
            break;
        }

        qSort(m_records.begin(), m_records.end(), lessThan);
    }

private:
    Column m_sortCol;
    Qt::SortOrder m_sortOrder;
    QVector<QnBusinessActionDataListPtr> m_events;
    QVector<QnLightBusinessActionP> m_records;
    int m_size;
    static int m_eventTypeToLexOrder[256]; // TODO: #Elric evil statics. Make non-static.
    static int m_actionTypeToLexOrder[256];
};

int QnEventLogModel::DataIndex::m_eventTypeToLexOrder[256];
int QnEventLogModel::DataIndex::m_actionTypeToLexOrder[256];


// -------------------------------------------------------------------------- //
// QnEventLogModel
// -------------------------------------------------------------------------- //
QnEventLogModel::QnEventLogModel(QObject *parent):
    base_type(parent),
    m_linkBrush(QPalette().link())
{
    m_linkFont.setUnderline(true);
    m_index = new DataIndex();

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnEventLogModel::at_resource_removed);
}

QnEventLogModel::~QnEventLogModel() {
    delete m_index;
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
        }
        resId = action.actionParams.actionResourceId;
    default:
        break;
    }

    return qnResIconCache->icon(getResourceById(resId));
}

QString QnEventLogModel::getResourceNameString(const QnUuid &id) {
    return getResourceName(getResourceById(id));
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

QString QnEventLogModel::textData(const Column& column,const QnBusinessActionData& action) {
    switch(column) {
    case DateTimeColumn: {
        qint64 timestampUsec = action.eventParams.eventTimestampUsec;
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestampUsec/1000);
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
        else
            return getResourceNameString(action.actionParams.actionResourceId);
    }
    case DescriptionColumn: {
        QnBusiness::EventType eventType = action.eventParams.eventType;
        QString result;

        if (eventType == QnBusiness::CameraMotionEvent) {
            if (action.hasFlags(QnBusinessActionData::MotionExists))
                result = tr("Motion video");
        }
        else {
            result = QnBusinessStringsHelper::eventDetails(action.eventParams, 1, lit("\n"));
        }

        if (!QnBusiness::hasToggleState(eventType)) {
            int count = action.aggregationCount;
            if (count > 1) {
                QString countString = tr("%1 times").arg(count);
                result += lit(" (%1)").arg(countString);
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
        QnResourcePtr resource = eventResource(index.row());
        if (resource)
            result << resource;
    }
    result << qnResPool->getResources<QnResource>(action.eventParams.metadata.cameraRefs);
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
        if (index.column() != DescriptionColumn)
            return QVariant();
        // else go to Qt::DisplayRole
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
