#include "event_log_model.h"

#include <cassert>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include "core/resource/resource.h"
#include "business/events/reasoned_business_event.h"
#include "core/resource_managment/resource_pool.h"
#include "ui/style/resource_icon_cache.h"
#include "business/business_strings_helper.h"
#include "client/client_globals.h"
#include <utils/math/math.h>

class QnEventLogModel::DataIndex
{
public:
    DataIndex(): m_sortCol(DateTimeColumn), m_sortOrder(Qt::AscendingOrder)
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
            events.insert(BusinessEventType::toString(BusinessEventType::Value(i)), i);
            m_eventTypeToLexOrder[i] = 255; // put undefined events to the end of the list
        }
        int cnt = 0;
        for(QMap<QString, int>::const_iterator itr = events.begin(); itr != events.end(); ++itr)
            m_eventTypeToLexOrder[itr.value()] = cnt++;

        // action types to lex order
        QMap<QString, int> actions;
        for (int i = 0; i < 256; ++i) {
            actions.insert(BusinessActionType::toString(BusinessActionType::Value(i)), i);
            m_actionTypeToLexOrder[i] = 255; // put undefined actions to the end of the list
        }
        cnt = 0;
        for(QMap<QString, int>::const_iterator itr = events.begin(); itr != events.end(); ++itr)
            m_actionTypeToLexOrder[itr.value()] = cnt++;
    }

    void setSort(int column, Qt::SortOrder order)
    { 
        if ((Column) column == m_sortCol && order == m_sortOrder)
            return;
            
        m_sortCol = (Column) column;
        m_sortOrder = order;

        updateIndex();
    }

    void setEvents(const QVector<QnLightBusinessActionVectorPtr>& events) 
    { 
        m_events = events;
        m_size = 0;
        for (int i = 0; i < events.size(); ++i)
            m_size += events[i]->size();

        updateIndex();
    }

    QVector<QnLightBusinessActionVectorPtr> events() const
    {
        return m_events;
    }

    void clear()
    {
        m_events.clear();
    }

    int size() const
    {
        return m_size;
    }

    /*
    * Reorder event types to lexographical order (for sorting)
    */
    static int toLexEventType(BusinessEventType::Value eventType)
    {
        return m_eventTypeToLexOrder[((int) eventType) & 0xff];
    }

    /*
    * Reorder actions types to lexographical order (for sorting)
    */
    static int toLexActionType(BusinessActionType::Value actionType)
    {
        return m_actionTypeToLexOrder[((int) actionType) & 0xff];
    }

    inline QnLightBusinessAction& at(int row)
    {
        return *m_records[row];
    }

    // comparators

    typedef bool (*LessFunc)(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2);

    static bool lessThanTimestamp(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2)
    {
        return d1->timestamp() < d2->timestamp();
    }

    static bool lessThanEventType(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2)
    {
        return QnEventLogModel::DataIndex::toLexEventType(d1->eventType()) < QnEventLogModel::DataIndex::toLexEventType(d2->eventType());
    }

    static bool lessThanActionType(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2)
    {
        return QnEventLogModel::DataIndex::toLexActionType(d1->actionType()) < QnEventLogModel::DataIndex::toLexActionType(d2->actionType());
    }

    static bool lessLexographic(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2)
    {
        return d1->compareString() < d2->compareString();
    }

    static bool greatThanTimestamp(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2)
    {
        return d1->timestamp() >= d2->timestamp();
    }

    static bool greatThanEventType(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2)
    {
        return QnEventLogModel::DataIndex::toLexEventType(d1->eventType()) >= QnEventLogModel::DataIndex::toLexEventType(d2->eventType());
    }

    static bool greatThanActionType(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2)
    {
        return QnEventLogModel::DataIndex::toLexActionType(d1->actionType()) >= QnEventLogModel::DataIndex::toLexActionType(d2->actionType());
    }

    static bool greatLexographic(QnLightBusinessAction* &d1, QnLightBusinessAction* &d2)
    {
        return d1->compareString() >= d2->compareString();
    }

    void updateIndex()
    {
        QTime t;
        t.restart();

        m_records.resize(m_size);
        if (m_records.isEmpty())
            return;

        QnLightBusinessAction** dst = &m_records[0];
        for (int i = 0; i < m_events.size(); ++i)
        {
            QnLightBusinessActionVector& data = *m_events[i].data();
            for (int j = 0; j < data.size(); ++j)
                *dst++ = &data[j];
        }

        LessFunc lessThan;
        if (m_sortOrder == Qt::AscendingOrder) {
            switch(m_sortCol)
            {
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
                    m_records[i]->setCompareString(m_owner->textData(m_sortCol, *m_records[i]));
                break;
            }
        }
        else {
            switch(m_sortCol)
            {
            case DateTimeColumn:
                lessThan = &greatThanTimestamp;
                break;
            case EventColumn:
                lessThan = &greatThanEventType;
                break;
            case ActionColumn:
                lessThan = &greatThanActionType;
                break;
            default:
                lessThan = &lessLexographic;
                for (int i = 0; i < m_records.size(); ++i)
                    m_records[i]->setCompareString(m_owner->textData(m_sortCol, *m_records[i]));
                break;
            }
        }

        qSort(m_records.begin(), m_records.end(), lessThan);

        int elapsed = t.elapsed();
        qWarning() << "sort time=" << elapsed;
    }

private:
    Column m_sortCol;
    Qt::SortOrder m_sortOrder;
    QVector<QnLightBusinessActionVectorPtr> m_events;
    QVector<QnLightBusinessAction*> m_records;
    int m_size;
    QnEventLogModel* m_owner;
    static int m_eventTypeToLexOrder[256];
    static int m_actionTypeToLexOrder[256];
};

int QnEventLogModel::DataIndex::m_eventTypeToLexOrder[256];
int QnEventLogModel::DataIndex::m_actionTypeToLexOrder[256];

// ---------------------- EventLogModel --------------------------

QnEventLogModel::QnEventLogModel(QObject *parent):
    base_type(parent),
    m_linkBrush(QPalette().link())
{
    m_linkFont.setUnderline(true);
    m_index = new DataIndex();
    rebuild();
}

QnEventLogModel::~QnEventLogModel() {
    delete m_index;
}

const QVector<QnLightBusinessActionVectorPtr> &QnEventLogModel::events() const {
    return m_index->events();
}

void QnEventLogModel::setEvents(const QVector<QnLightBusinessActionVectorPtr> &events)
{
    m_index->setEvents(events);
    rebuild();
}

QList<QnEventLogModel::Column> QnEventLogModel::columns() const {
    return m_columns;
}

void QnEventLogModel::setColumns(const QList<Column> &columns) {
    if(m_columns == columns)
        return;

    foreach(Column column, columns) {
        if(column < 0 || column >= ColumnCount) {
            qnWarning("Invalid column '%1'.", static_cast<int>(column));
            return;
        }
    }

    m_columns = columns;

    /* Fill header. */
    setColumnCount(m_columns.size());
    for(int c = 0; c < m_columns.size(); c++)
        setHeaderData(c, Qt::Horizontal, columnTitle(m_columns[c]));

    rebuild();
}

QString QnEventLogModel::columnTitle(Column column) {
    switch(column) {
    case DateTimeColumn:        return tr("Date/Time");
    case EventColumn:           return tr("Event");
    case EventCameraColumn:     return tr("Source");
    case ActionColumn:          return tr("Action");
    case ActionCameraColumn:    return tr("Target");
    case DescriptionColumn:     return tr("Description");
    default:
        assert(false);
        return QString();
    }
}

void QnEventLogModel::clear()
{
    m_index->clear();
    QStandardItemModel::clear();
}

QVariant QnEventLogModel::fontData(const Column& column, const QnLightBusinessAction &action) const
{
    if (column == DescriptionColumn) {
        BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
        if (eventType == BusinessEventType::Camera_Motion)
            return m_linkFont;
    }

    return QVariant();
}

QVariant QnEventLogModel::foregroundData(const Column& column, const QnLightBusinessAction &action) const
{
    if (column == DescriptionColumn) {
        BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
        if (eventType == BusinessEventType::Camera_Motion)
            return m_linkBrush;
    }

    return QVariant();
}

QVariant QnEventLogModel::mouseCursorData(const Column& column, const QnLightBusinessAction &action) const
{
    if (column == DescriptionColumn) {
        BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
        if (eventType == BusinessEventType::Camera_Motion)
            return Qt::PointingHandCursor;
    }

    return QVariant();
}


QVariant QnEventLogModel::iconData(const Column& column, const QnLightBusinessAction &action) const
{

    switch(column) {
        case EventCameraColumn: 
        {
            QnId eventResId = action.getRuntimeParams().getEventResourceId();
            QnResourcePtr eventResource = qnResPool->getResourceById(eventResId);
            if (eventResource)
                return qnResIconCache->icon(eventResource->flags(), eventResource->getStatus());
            break;
        }
        case ActionCameraColumn: 
        {
            QnId actionResId = action.getRuntimeParams().getActionResourceId();
            QnResourcePtr actionResource = qnResPool->getResourceById(actionResId);
            if (actionResource)
                return qnResIconCache->icon(actionResource->flags(), actionResource->getStatus());
            break;
        }
    	default:
        	break;
    }
    return QVariant();
}

QVariant QnEventLogModel::resourceData(const Column& column, const QnLightBusinessAction &action) const
{
    switch(column) {
        case EventCameraColumn: 
            return action.getRuntimeParams().getEventResourceId();
        case ActionCameraColumn: 
            return action.getRuntimeParams().getActionResourceId();
    }
    return QVariant();
}

QString QnEventLogModel::formatUrl(const QString& url) const
{
    if (url.indexOf(QLatin1String("://")) == -1)
        return url;
    else
        return QUrl(url).host();
}

QString QnEventLogModel::textData(const Column& column,const QnLightBusinessAction& action) const
{
    switch(column) 
    {
        case DateTimeColumn: {
            qint64 timestampUsec = action.getRuntimeParams().getEventTimestamp();
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestampUsec/1000);
            return dt.toString(Qt::SystemLocaleShortDate);
            break;
        }
        case EventColumn: {
            BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
            return BusinessEventType::toString(eventType);
            break;
        }
        case EventCameraColumn: {
            QnId eventResId = action.getRuntimeParams().getEventResourceId();
            QnResourcePtr eventResource = qnResPool->getResourceById(eventResId);
            if (eventResource)
                return QString(lit("%1 (%2)")).arg(eventResource->getName()).arg(formatUrl(eventResource->getUrl()));
            break;
        }
        case ActionColumn:
            return BusinessActionType::toString(action.actionType());
            break;
        case ActionCameraColumn: {
            QnId actionResId = action.getRuntimeParams().getActionResourceId();
            QnResourcePtr actionResource = qnResPool->getResourceById(actionResId);
            if (actionResource)
                return QString(lit("%1 (%2)")).arg(actionResource->getName()).arg(formatUrl(actionResource->getUrl()));
            break;
            }
        case DescriptionColumn: {
            BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
            QString result;

            switch (eventType) {
                case BusinessEventType::Camera_Disconnect:
                    break;
                case BusinessEventType::Camera_Input:
                    result = QnBusinessStringsHelper::eventTextString(eventType, action.getRuntimeParams());
                    break;
                case BusinessEventType::Camera_Motion:
                    result = lit("Click me to see video");
                    break;
                case BusinessEventType::Storage_Failure:
                case BusinessEventType::Network_Issue:
                case BusinessEventType::MediaServer_Failure: {
                    result = QnBusinessStringsHelper::eventReason(action.getRuntimeParams());
                    break;
                }
                case BusinessEventType::Camera_Ip_Conflict:
                case BusinessEventType::MediaServer_Conflict: {
                    result = QnBusinessStringsHelper::conflictString(action.getRuntimeParams(), QLatin1Char(' '));
                    break;
				}
            	default:
					break;
            }
            if (!BusinessEventType::hasToggleState(eventType)) {
                int cnt = action.getAggregationCount();
                if (cnt > 1)
                    result += QString(lit(" (%1 times)")).arg(cnt);
            }
            return result;
        }
    } 
    return QString();
}

void QnEventLogModel::sort(int column, Qt::SortOrder order)
{
    m_index->setSort(column, order);
}

QVariant QnEventLogModel::data ( const QModelIndex & index, int role) const
{
    const Column& column = m_columns[index.column()];
    const QnLightBusinessAction& action = m_index->at(index.row());
    
    switch(role)
    {
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
            return resourceData(column, action);
    }

    return QVariant();
}

BusinessEventType::Value QnEventLogModel::eventType(const QModelIndex & index) const
{
    if (index.isValid()) {
        const QnLightBusinessAction& action = m_index->at(index.row());
        return action.getRuntimeParams().getEventType();
    }
    else {
        return BusinessEventType::NotDefined;
    }
}

QnResourcePtr QnEventLogModel::eventResource(const QModelIndex & index) const
{
    if (index.isValid()) {
        const QnLightBusinessAction& action = m_index->at(index.row());
        return qnResPool->getResourceById(action.getRuntimeParams().getEventResourceId());
    }
    else {
        return QnResourcePtr();
    }
}

qint64 QnEventLogModel::eventTimestamp(const QModelIndex & index) const
{
    if (index.isValid()) {
        const QnLightBusinessAction& action = m_index->at(index.row());
        return action.timestamp();
    }
    else {
        return AV_NOPTS_VALUE;
    }
}

void QnEventLogModel::rebuild() 
{
    setRowCount(m_index->size());
}

QnLightBusinessActionVectorPtr QnEventLogModel::merge2(const QList <QnLightBusinessActionVectorPtr>& eventsList) const
{
    QnLightBusinessActionVectorPtr events1 = eventsList[0];
    QnLightBusinessActionVectorPtr events2 = eventsList[1];

    QnLightBusinessActionVectorPtr events = QnLightBusinessActionVectorPtr(new QnLightBusinessActionVector);
    events->resize(events1->size() + events2->size());

    int idx1 = 0, idx2 = 0;

    QnLightBusinessActionVector& v1  = *events1.data();
    QnLightBusinessAction* ptr1 = &v1[0];

    QnLightBusinessActionVector& v2  = *events2.data();
    QnLightBusinessAction* ptr2 = &v2[0];

    QnLightBusinessActionVector& vDst = *events.data();
    QnLightBusinessAction* dst  = &vDst[0];


    while (idx1 < events1->size() && idx2 < events2->size())
    {
        if (ptr1[idx1].timestamp() <= ptr2[idx2].timestamp())
            *dst++ = ptr1[idx1++];
        else
            *dst++ = ptr2[idx2++];
    }

    while (idx1 < events1->size())
        *dst++ = ptr1[idx1++];

    while (idx2 < events2->size())
        *dst++ = ptr2[idx2++];

    return events;
}

QnLightBusinessActionVectorPtr QnEventLogModel::mergeN(const QList <QnLightBusinessActionVectorPtr>& eventsList) const
{
    QnLightBusinessActionVectorPtr events = QnLightBusinessActionVectorPtr(new QnLightBusinessActionVector);

    QVector<int> idx;
    idx.resize(eventsList.size());

    QVector<QnLightBusinessAction*> ptr;
    QVector<QnLightBusinessAction*> ptrEnd;
    int totalSize = 0;
    for (int i = 0; i < eventsList.size(); ++i)
    {
        QnLightBusinessActionVector& tmp  = *eventsList[i].data();
        ptr << &tmp[0];
        ptrEnd << &tmp[0] + eventsList[i]->size();
        totalSize += eventsList[i]->size();
    }
    events->resize(totalSize);
    QnLightBusinessActionVector& vDst = *events.data();
    QnLightBusinessAction* dst  = &vDst[0];

    int curIdx = -1;
    do
    {
        qint64 curTimestamp = INT64_MAX;
        curIdx = -1;
        for (int i = 0; i < eventsList.size(); ++i)
        {
            if (ptr[i] < ptrEnd[i] && ptr[i]->timestamp() < curTimestamp) {
                curIdx = i;
                curTimestamp = ptr[i]->timestamp();
            }
        }
        if (curIdx >= 0) {
            *dst++ = *ptr[curIdx];
            ptr[curIdx]++;
        }
    } while (curIdx >= 0);

    return events;
}

QnLightBusinessActionVectorPtr QnEventLogModel::mergeEvents(const QList <QnLightBusinessActionVectorPtr>& eventsList) const
{
    if (eventsList.empty())
        return QnLightBusinessActionVectorPtr();
    else if (eventsList.size() == 1)
        return eventsList[0];
    else if (eventsList.size() == 2)
        return merge2(eventsList);
    else
        return mergeN(eventsList);
}
