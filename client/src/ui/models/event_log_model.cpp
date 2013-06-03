#include "event_log_model.h"

#include <cassert>

#include <QPalette>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include "core/resource/resource.h"
#include "business/events/reasoned_business_event.h"
#include "core/resource_managment/resource_pool.h"
#include "ui/style/resource_icon_cache.h"
#include "business/business_strings_helper.h"
#include "client/client_globals.h"
#include <utils/math/math.h>
#include "device_plugins/server_camera/server_camera.h"
#include "client/client_settings.h"

typedef QnBusinessActionData* QnLightBusinessActionP;

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

    inline QnBusinessActionData& at(int row)
    {
        return m_sortOrder == Qt::AscendingOrder ? *m_records[row] : *m_records[m_size-1-row];
    }

    // comparators

    typedef bool (*LessFunc)(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2);

    static bool lessThanTimestamp(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2)
    {
        return d1->timestamp() < d2->timestamp();
    }

    static bool lessThanEventType(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2)
    {
        int r1 = QnEventLogModel::DataIndex::toLexEventType(d1->eventType());
        int r2 = QnEventLogModel::DataIndex::toLexEventType(d2->eventType());
        if (r1 < r2)
            return true;
        else if (r1 > r2)
            return false;
        else
            return lessThanTimestamp(d1, d2);
    }

    static bool lessThanActionType(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2)
    {
        int r1 = QnEventLogModel::DataIndex::toLexActionType(d1->actionType());
        int r2 = QnEventLogModel::DataIndex::toLexActionType(d2->actionType());
        if (r1 < r2)
            return true;
        else if (r1 > r2)
            return false;
        else
            return lessThanTimestamp(d1, d2);
    }

    static bool lessLexographic(const QnLightBusinessActionP &d1, const QnLightBusinessActionP &d2)
    {
        int rez = d1->compareString().compare(d2->compareString());
        if (rez < 0)
            return true;
        else if (rez > 0)
            return false;
        else
            return lessThanTimestamp(d1, d2);
    }

    void updateIndex()
    {
        QTime t;
        t.restart();

        m_records.resize(m_size);
        if (m_records.isEmpty())
            return;

        QnLightBusinessActionP* dst = &m_records[0];
        for (int i = 0; i < m_events.size(); ++i)
        {
            QnBusinessActionDataList& data = *m_events[i].data();
            for (int j = 0; j < data.size(); ++j)
                *dst++ = &data[j];
        }

        LessFunc lessThan;
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
                m_records[i]->setCompareString(QnEventLogModel::textData(m_sortCol, *m_records[i]));
            break;
        }

        qSort(m_records.begin(), m_records.end(), lessThan);

        int elapsed = t.elapsed();
        qDebug() << "sort time=" << elapsed;
    }

private:
    Column m_sortCol;
    Qt::SortOrder m_sortOrder;
    QVector<QnBusinessActionDataListPtr> m_events;
    QVector<QnLightBusinessActionP> m_records;
    int m_size;
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
}

QnEventLogModel::~QnEventLogModel() {
    delete m_index;
}

void QnEventLogModel::setEvents(const QVector<QnBusinessActionDataListPtr> &events)
{
    m_index->setEvents(events);
    rebuild();
}

QList<QnEventLogModel::Column> QnEventLogModel::columns() const {
    return m_columns;
}

void QnEventLogModel::setColumns(const QList<Column> &columns) 
{
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

QVariant QnEventLogModel::fontData(const Column& column, const QnBusinessActionData &action) const
{
    if (column == DescriptionColumn) {
        BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
        if (eventType == BusinessEventType::Camera_Motion)
            return m_linkFont;
    }

    return QVariant();
}

QVariant QnEventLogModel::foregroundData(const Column& column, const QnBusinessActionData &action) const
{
    if (column == DescriptionColumn) {
        BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
        if (eventType == BusinessEventType::Camera_Motion)
            return m_linkBrush;
    }

    return QVariant();
}

QVariant QnEventLogModel::mouseCursorData(const Column& column, const QnBusinessActionData &action)
{
    if (column == DescriptionColumn && action.hasFlags(QnBusinessActionData::MotionExists)) {
        BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
        if (eventType == BusinessEventType::Camera_Motion)
            return Qt::PointingHandCursor;
    }

    return QVariant();
}

QnResourcePtr QnEventLogModel::getResource(const QModelIndex& idx) const
{
    return getResourceById(data(idx, Qn::ResourceRole).toInt());
}


QnResourcePtr QnEventLogModel::getResourceById(const QnId& id)
{
    QnResourcePtr resource = qnResPool->getResourceById(id);
    if (resource && resource->isDisabled())
    {
        QnServerCameraPtr localCam = resource.dynamicCast<QnServerCamera>();
        if (localCam) {
            localCam = localCam->findEnabledSubling();
            if (localCam)
                return localCam;
        }
    }
    return resource;
}

QVariant QnEventLogModel::iconData(const Column& column, const QnBusinessActionData &action)
{
    QnId resId;
    switch(column) {
        case EventCameraColumn: 
            resId = action.getRuntimeParams().getEventResourceId();
            break;
        case ActionCameraColumn: 
            {
                BusinessActionType::Value actionType = action.actionType();
                if (actionType == BusinessActionType::SendMail) {
                    if (!action.getParams().getEmailAddress().isEmpty()) {
                        if (action.getParams().getEmailAddress().indexOf(L';') > 0)
                            return qnResIconCache->icon(QnResourceIconCache::Users);
                        else
                            return qnResIconCache->icon(QnResourceIconCache::User);
                    }
                    else {
                        return QVariant();
                    }
                }
                else if (actionType == BusinessActionType::ShowPopup) {
                    if (action.getParams().getUserGroup() == QnBusinessActionParameters::AdminOnly)
                        return qnResIconCache->icon(QnResourceIconCache::User);
                    else
                        return qnResIconCache->icon(QnResourceIconCache::Users);
                }
            }
            resId = action.getRuntimeParams().getActionResourceId();
    	default:
        	break;
    }

    QnResourcePtr res = getResourceById(resId);
    if (res)
        return qnResIconCache->icon(res->flags(), res->getStatus());
    else
        return QVariant();
}

QVariant QnEventLogModel::resourceData(const Column& column, const QnBusinessActionData &action)
{
    switch(column) {
        case EventCameraColumn: 
            return action.getRuntimeParams().getEventResourceId();
        case ActionCameraColumn: 
            return action.getRuntimeParams().getActionResourceId();
        default:
            return QVariant();
    }
    return QVariant();
}

QString QnEventLogModel::formatUrl(const QString& url)
{
    int prefix = url.indexOf(QLatin1String("://"));
    if (prefix == -1)
        return url;
    else {
        prefix += 3;
        int hostEnd = url.indexOf(QLatin1Char(':'), prefix);
        if (hostEnd == -1) {
            hostEnd = url.indexOf(QLatin1Char('/'), prefix);
            if (hostEnd == -1)
                hostEnd = url.indexOf(QLatin1Char('?'), prefix);
        }

        if (hostEnd != -1)
            return url.mid(prefix, hostEnd - prefix);
    }
    return url;
}

QString QnEventLogModel::getResourceNameString(QnId id)
{
    QString result;
    QnResourcePtr res = getResourceById(id);
    if (res) {
        result = res->getName();
        if (qnSettings->isIpShownInTree())
            result += QString(lit(" (%2)")).arg(formatUrl(res->getUrl()));
    }
    return result;
}

QString QnEventLogModel::getUserGroupString(QnBusinessActionParameters::UserGroup value)
{
    switch (value) {
        case QnBusinessActionParameters::EveryOne:
            return tr("All users");
        case QnBusinessActionParameters::AdminOnly:
            return tr("Administrators Only");
        default:
            return QString();
    }
    return QString();
}

QString QnEventLogModel::textData(const Column& column,const QnBusinessActionData& action)
{
    switch(column) 
    {
        case DateTimeColumn: {
            qint64 timestampUsec = action.getRuntimeParams().getEventTimestamp();
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestampUsec/1000);
            return dt.toString(Qt::SystemLocaleShortDate);
            break;
        }
        case EventColumn:
            return BusinessEventType::toString(action.getRuntimeParams().getEventType());
            break;
        case EventCameraColumn:
            return getResourceNameString(action.getRuntimeParams().getEventResourceId());
            break;
        case ActionColumn:
            return BusinessActionType::toString(action.actionType());
            break;
        case ActionCameraColumn: {
            BusinessActionType::Value actionType = action.actionType();
            if (actionType == BusinessActionType::SendMail)
                return action.getParams().getEmailAddress();
            else if (actionType == BusinessActionType::ShowPopup)
                return getUserGroupString(action.getParams().getUserGroup());
            else
                return getResourceNameString(action.getRuntimeParams().getActionResourceId());
            break;
        }
        case DescriptionColumn: {
            BusinessEventType::Value eventType = action.getRuntimeParams().getEventType();
            QString result;

            if (eventType == BusinessEventType::Camera_Motion) {
                if (action.hasFlags(QnBusinessActionData::MotionExists))
                    result = lit("Motion video");
            }
            else {
                result = QnBusinessStringsHelper::eventParamsString(eventType, action.getRuntimeParams());
            }

            if (!BusinessEventType::hasToggleState(eventType)) {
                int cnt = action.getAggregationCount();
                if (cnt > 1)
                    result += QString(lit(" (%1 times)")).arg(cnt);
            }
            return result;
        }
        default:
            break;
    }
    return QString();
}

void QnEventLogModel::sort(int column, Qt::SortOrder order)
{
    m_index->setSort(column, order);
    rebuild();
}

QString QnEventLogModel::motionUrl(Column column, const QnBusinessActionData& action)
{
    if (column != DescriptionColumn || !action.hasFlags(QnBusinessActionData::MotionExists))
        return QString();

    if (action.getRuntimeParams().getEventType() != BusinessEventType::Camera_Motion)
        return QString();
    return QnBusinessStringsHelper::motionUrl(action.getRuntimeParams());
}

bool QnEventLogModel::hasMotionUrl(const QModelIndex & index) const
{
    if (!index.isValid() || index.column() != DescriptionColumn)
        return false;

    const QnBusinessActionData& action = m_index->at(index.row());
    if (!action.hasFlags(QnBusinessActionData::MotionExists))
        return false;
    if (!action.getRuntimeParams().getEventResourceId())
        return false;
    
    return true;
}

QVariant QnEventLogModel::data( const QModelIndex & index, int role) const
{
    if (index.row() >= m_index->size())
        return QVariant();

    const Column& column = m_columns[index.column()];
    const QnBusinessActionData& action = m_index->at(index.row());
    
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
        case Qn::DisplayHtmlRole: {
            QString text = textData(column, action);
            QString url = motionUrl(column, action);
            if (url.isEmpty())
                return text;
            else 
                return QString(lit("<a href=\"%1\">%2</a>")).arg(url, text);
        }
        default:
            return QVariant();
    }

    return QVariant();
}

BusinessEventType::Value QnEventLogModel::eventType(int row) const
{
    if (row >= 0) {
        const QnBusinessActionData& action = m_index->at(row);
        return action.getRuntimeParams().getEventType();
    }
    else {
        return BusinessEventType::NotDefined;
    }
}

QnResourcePtr QnEventLogModel::eventResource(int row) const
{
    if (row >= 0) {
        const QnBusinessActionData& action = m_index->at(row);
        return qnResPool->getResourceById(action.getRuntimeParams().getEventResourceId());
    }
    else {
        return QnResourcePtr();
    }
}

qint64 QnEventLogModel::eventTimestamp(int row) const
{
    if (row >= 0) {
        const QnBusinessActionData& action = m_index->at(row);
        return action.timestamp();
    }
    else {
        return AV_NOPTS_VALUE;
    }
}

void QnEventLogModel::rebuild()
{
    setRowCount(m_index->size());
    reset();
}
