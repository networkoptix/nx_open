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

QnEventLogModel::QnEventLogModel(QObject *parent):
    base_type(parent),
    m_linkBrush(QPalette().link())
{
    m_events = QnLightBusinessActionVectorPtr(new QnLightBusinessActionVector());
    m_linkFont.setUnderline(true);
    rebuild();
}

QnEventLogModel::~QnEventLogModel() {
    return;
}

const QnLightBusinessActionVectorPtr &QnEventLogModel::events() const {
    return m_events;
}

void QnEventLogModel::setEvents(const QnLightBusinessActionVectorPtr& events) {
    m_events = events;
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
    m_events.clear();
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

QVariant QnEventLogModel::textData(const Column& column,const QnLightBusinessAction& action) const
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
            }
            if (!BusinessEventType::hasToggleState(eventType)) {
                int cnt = action.getAggregationCount();
                if (cnt > 1)
                    result += QString(lit(" (%1 times)")).arg(cnt);
            }
            return result;
        }
    } 
    return QVariant();
}

QVariant QnEventLogModel::data ( const QModelIndex & index, int role) const
{
    const Column& column = m_columns[index.column()];
    const QnLightBusinessAction& action = m_events->at(index.row());
    
    switch(role)
    {
        case Qt::DisplayRole:
            return textData(column, action);
        case Qt::DecorationRole:
            return iconData(column, action);
        case Qt::FontRole:
            return fontData(column, action);
        case Qt::ForegroundRole:
            return foregroundData(column, action);
        case Qn::ItemMouseCursorRole:
            return mouseCursorData(column, action);
    }

    return QVariant();
}

/*
QVariant QnEventLogModel::data ( const QModelIndex & index, int role) const
{
    QStandardItem* i = item(index.row(), index.column());
    if (i == 0) {
        i = createItem(m_columns[index.column()], m_events[index.row()]);
        const_cast<QnEventLogModel*>(this)->setItem(index.row(), index.column(), i);
    }
    return i->data(role);
}
*/

void QnEventLogModel::rebuild() 
{
    QTime t;
    t.restart();

    QStandardItemModel::clear();
    if(m_columns.isEmpty())
        return;

    /* Fill header. */
    setColumnCount(m_columns.size());
    for(int c = 0; c < m_columns.size(); c++)
        setHeaderData(c, Qt::Horizontal, columnTitle(m_columns[c]));

    setRowCount(m_events ? m_events->size() : 0);
    /*
    // Fill data.
    for(int r = 0; r < m_events.size(); r++) {
        QList<QStandardItem *> items;
        for(int c = 0; c < m_columns.size(); c++)
            items.push_back(createItem(m_columns[c], m_events[r]));

        appendRow(items);
    }
    */

    qDebug() << Q_FUNC_INFO << "time=" << t.elapsed();
}

/*
QnLightBusinessAction QnEventLogModel::getEvent(const QModelIndex &index) const {
    bool ok = false;
    int r = index.sibling(index.row(), 0).data(Qt::UserRole).toInt(&ok);
    if(!ok || r < 0 || r >= m_events.size())
        return QnLightBusinessActionPtr();

    return m_events[r];
}
*/