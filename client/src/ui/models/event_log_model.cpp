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
    m_linkFont.setUnderline(true);
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

QStandardItem *QnEventLogModel::createItem(Column column, const QnAbstractBusinessActionPtr &action) {
    QStandardItem *item = new QStandardItem();

    qint64 timestampUsec;
    BusinessEventType::Value eventType;
    QnResourcePtr eventResource;
    QnResourcePtr actionResource;
    QnId eventResId;
    QnId actionResId;
    BusinessActionType::Value actionType;
    QDateTime dt;

    switch(column) {
    case DateTimeColumn:
        timestampUsec = QnBusinessEventRuntime::getEventTimestamp(action->getRuntimeParams());
        dt = QDateTime::fromMSecsSinceEpoch(timestampUsec/1000);
        item->setText(dt.toString(Qt::SystemLocaleShortDate));
        break;
    case EventColumn:
        eventType = QnBusinessEventRuntime::getEventType(action->getRuntimeParams());
        item->setText(BusinessEventType::toString(eventType));
        break;
    case EventCameraColumn:
        eventResId = QnBusinessEventRuntime::getEventResourceId(action->getRuntimeParams());
        eventResource = qnResPool->getResourceById(eventResId);
        if (eventResource) {
            item->setText(eventResource->getName());
            item->setIcon(qnResIconCache->icon(eventResource->flags(), eventResource->getStatus()));
        }
        break;
    case ActionColumn:
        item->setText(BusinessActionType::toString(action->actionType()));
        break;
    case ActionCameraColumn:
        actionResId = QnBusinessActionRuntime::getActionResourceId(action->getRuntimeParams());
        actionResource = qnResPool->getResourceById(eventResId);
        if (actionResource) {
            item->setText(actionResource->getName());
            item->setIcon(qnResIconCache->icon(actionResource->flags(), actionResource->getStatus()));
        }
        break;
    case DescriptionColumn:
            eventType = QnBusinessEventRuntime::getEventType(action->getRuntimeParams());
            switch (eventType) {
                case BusinessEventType::Camera_Disconnect:
                case BusinessEventType::Camera_Input:
                    break;
                case BusinessEventType::Camera_Motion:
                    item->setText(tr("Click me to see video"));
                    item->setFont(m_linkFont);
                    item->setForeground(m_linkBrush);
                    item->setData(QnBusinessStringsHelper::motionUrl(action->getRuntimeParams()), ItemLinkRole);
                    item->setData(Qt::PointingHandCursor, Qn::ItemMouseCursorRole);
                    break;
                case BusinessEventType::Storage_Failure:
                case BusinessEventType::Network_Issue:
                case BusinessEventType::MediaServer_Failure:
                    item->setText(QnBusinessStringsHelper::eventReason(action.data()));
                    break;
                case BusinessEventType::Camera_Ip_Conflict:
                case BusinessEventType::MediaServer_Conflict:
                    item->setText(QnBusinessStringsHelper::conflictString(action->getRuntimeParams()));
                    break;
                default:
                    return false;
            }
        break;
    default:
        assert(false);
    }

    return item;
}

void QnEventLogModel::clear()
{
    m_events.clear();
    QStandardItemModel::clear();
}

void QnEventLogModel::rebuild() {
    QStandardItemModel::clear();
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
