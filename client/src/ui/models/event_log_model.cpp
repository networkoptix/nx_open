#include "event_log_model.h"

#include <cassert>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include "core/resource/resource.h"
#include "business/events/reasoned_business_event.h"
#include "core/resource_managment/resource_pool.h"

QnEventLogModel::QnEventLogModel(QObject *parent):
    base_type(parent)
{
    rebuild();
}

QnEventLogModel::~QnEventLogModel() {
    return;
}

const QnAbstractBusinessActionList &QnEventLogModel::events() const {
    return m_events;
}

void QnEventLogModel::setEvents(const QnAbstractBusinessActionList &events) {
    if(m_events == events)
        return;

    m_events = events;

    rebuild();
}

void QnEventLogModel::addEvents(const QnAbstractBusinessActionList &events) {
    m_events += events;
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
    case EventCameraColumn:     return tr("Event camera");
    case ActionColumn:          return tr("Action");
    case ActionCameraColumn:    return tr("Action camera");
    case RepeatCountColumn:     return tr("Repeat count");
    case DescriptionColumn:     return tr("Description");
    default:
        assert(false);
        return QString();
    }
}

QStandardItem *QnEventLogModel::createItem(Column column, const QnAbstractBusinessActionPtr &action) {
    QStandardItem *item = new QStandardItem();

    qint64 timestampUsec;
    BusinessEventType::Value eventType;
    QnResourcePtr eventResource;
    QnResourcePtr actionResource;
    QnId eventResId;
    QnId actionResId;
    BusinessActionType::Value actionType;

    switch(column) {
    case DateTimeColumn:
        timestampUsec = QnBusinessEventRuntime::getEventTimestamp(action->getRuntimeParams());
        item->setText(QDateTime::fromMSecsSinceEpoch(timestampUsec).toString());
        break;
    case EventColumn:
        eventType = QnBusinessEventRuntime::getEventType(action->getRuntimeParams());
        item->setText(BusinessEventType::toString(eventType));
        break;
    case EventCameraColumn:
        eventResId = QnBusinessEventRuntime::getEventResourceId(action->getRuntimeParams());
        eventResource = qnResPool->getResourceById(eventResId);
        if (eventResource)
            item->setText(eventResource->getName());
        break;
    case ActionColumn:
        item->setText(BusinessActionType::toString(action->actionType()));
        break;
    case ActionCameraColumn:
        actionResId = QnBusinessActionRuntime::getActionResourceId(action->getRuntimeParams());
        actionResource = qnResPool->getResourceById(eventResId);
        if (actionResource)
            item->setText(actionResource->getName());
        break;
    case RepeatCountColumn:
        item->setText(QString::number(action->getAggregationCount()));
        break;
    case DescriptionColumn:
        item->setText(lit("fill me"));
        break;
    default:
        assert(false);
    }

    return item;
}

void QnEventLogModel::rebuild() {
    clear();
    if(m_columns.isEmpty())
        return;

    /* Fill header. */
    setColumnCount(m_columns.size());
    for(int c = 0; c < m_columns.size(); c++)
        setHeaderData(c, Qt::Horizontal, columnTitle(m_columns[c]));

    /* Fill data. */
    for(int r = 0; r < m_events.size(); r++) {
        QList<QStandardItem *> items;
        for(int c = 0; c < m_columns.size(); c++)
            items.push_back(createItem(m_columns[c], m_events[r]));
        items[0]->setData(r, Qt::UserRole);

        appendRow(items);
    }
}

QnAbstractBusinessActionPtr QnEventLogModel::getEvent(const QModelIndex &index) const {
    bool ok = false;
    int r = index.sibling(index.row(), 0).data(Qt::UserRole).toInt(&ok);
    if(!ok || r < 0 || r >= m_events.size())
        return QnAbstractBusinessActionPtr();

    return m_events[r];
}
