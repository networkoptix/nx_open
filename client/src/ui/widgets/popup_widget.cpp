#include "popup_widget.h"
#include "ui_popup_widget.h"

#include <business/events/conflict_business_event.h>

#include <core/resource/resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/style/resource_icon_cache.h>

#include <utils/common/synctime.h>
#include <utils/settings.h>

namespace {

    enum ItemDataRole {
        ResourceIdRole = Qt::UserRole + 1,
        EventCountRole,
        EventTimeRole,
        EventReasonRole,
        ConflictsRole

    };

}

QnPopupWidget::QnPopupWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupWidget),
    m_eventType(BusinessEventType::BE_NotDefined)
{
    ui->setupUi(this);
    connect(ui->okButton, SIGNAL(clicked()), this, SLOT(at_okButton_clicked()));

    m_model = new QStandardItemModel(this);
    ui->eventsTreeView->setModel(m_model);
}

QnPopupWidget::~QnPopupWidget()
{
}

void QnPopupWidget::at_okButton_clicked() {
    emit closed(m_eventType, ui->ignoreCheckBox->isChecked());
}

void QnPopupWidget::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    if (m_eventType == BusinessEventType::BE_NotDefined) //not initialized
        initWidget(QnBusinessEventRuntime::getEventType(businessAction->getRuntimeParams()));
    updateTreeModel(businessAction);
    ui->eventsTreeView->expandAll();
}

void QnPopupWidget::initWidget(BusinessEventType::Value eventType) {
    QList<QWidget*> headerLabels;
    headerLabels << ui->warningLabel << ui->importantLabel << ui->notificationLabel;

    foreach (QWidget* w, headerLabels)
        w->setVisible(false);

    m_eventType = eventType;
    switch (m_eventType) {
        case BusinessEventType::BE_NotDefined:
            return;

        case BusinessEventType::BE_Camera_Input:
        case BusinessEventType::BE_Camera_Disconnect:
            ui->importantLabel->setVisible(true);
            break;

        case BusinessEventType::BE_Storage_Failure:
        case BusinessEventType::BE_Network_Issue:
        case BusinessEventType::BE_Camera_Ip_Conflict:
        case BusinessEventType::BE_MediaServer_Failure:
        case BusinessEventType::BE_MediaServer_Conflict:
            ui->warningLabel->setVisible(true);
            break;

        //case BusinessEventType::BE_Camera_Motion:
        //case BusinessEventType::BE_UserDefined:
        default:
            ui->notificationLabel->setVisible(true);
            break;
    }
    ui->eventLabel->setText(BusinessEventType::toString(eventType));
}

void QnPopupWidget::updateTreeModel(const QnAbstractBusinessActionPtr &businessAction) {
    switch (m_eventType) {
        case BusinessEventType::BE_Camera_Disconnect:
        case BusinessEventType::BE_Camera_Input:
        case BusinessEventType::BE_Camera_Motion:
            updateSimpleTree(businessAction);
            break;
        case BusinessEventType::BE_Storage_Failure:
        case BusinessEventType::BE_Network_Issue:
        case BusinessEventType::BE_MediaServer_Failure:
            updateReasonTree(businessAction);
            break;
        case BusinessEventType::BE_Camera_Ip_Conflict:
        case BusinessEventType::BE_MediaServer_Conflict:
            updateConflictTree(businessAction);
            break;
        default:
            break;
    }
}

QString QnPopupWidget::getEventTime(const QnBusinessParams &eventParams) {
    qint64 eventTimestamp = QnBusinessEventRuntime::getEventTimestamp(eventParams);
    if (eventTimestamp == 0)
        eventTimestamp = qnSyncTime->currentUSecsSinceEpoch();

    return QDateTime::fromMSecsSinceEpoch(eventTimestamp/1000).toString(QLatin1String("hh:mm:ss"));
}

void QnPopupWidget::updateSimpleTree(const QnAbstractBusinessActionPtr &businessAction) {
    int resourceId = QnBusinessEventRuntime::getEventResourceId(businessAction->getRuntimeParams());

    QStandardItem *root = m_model->invisibleRootItem();
    QStandardItem *item = NULL;

    for (int i = 0; i < root->rowCount(); i++) {
        QStandardItem* child = root->child(i);
        if (child->data(ResourceIdRole).toInt() != resourceId)
            continue;

        item = child;
        break;
    }
    if (item == NULL) {
        QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
        if (!resource)
            return;

        item = new QStandardItem();
        item->setText(resource->getName());
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setData(resourceId, ResourceIdRole);
        item->setData(0, EventCountRole);
        item->setData(getEventTime(businessAction->getRuntimeParams()), EventTimeRole);
        root->appendRow(item);
    }

    item->removeRows(0, item->rowCount());

    int count = item->data(EventCountRole).toInt() + qMax(businessAction->getAggregationCount(), 1);
    item->setData(count, EventCountRole);
    QString timeStamp = item->data(EventTimeRole).toString();
    if (count == 1)
        item->appendRow(new QStandardItem(tr("at %1").arg(timeStamp)));
    else
        item->appendRow(new QStandardItem(tr("%1 times since %2").arg(count).arg(timeStamp)));
}

void QnPopupWidget::updateReasonTree(const QnAbstractBusinessActionPtr &businessAction) {
    int resourceId = QnBusinessEventRuntime::getEventResourceId(businessAction->getRuntimeParams());
    QString eventReason = QnBusinessEventRuntime::getEventReason(businessAction->getRuntimeParams());

    QStandardItem *root = m_model->invisibleRootItem();
    QStandardItem *item = NULL;

    for (int i = 0; i < root->rowCount(); i++) {
        QStandardItem* child = root->child(i);
        if (child->data(ResourceIdRole).toInt() != resourceId)
            continue;
        if (child->data(EventReasonRole).toString() != eventReason)
            continue;

        item = child;
        break;
    }
    if (item == NULL) {
        QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
        if (!resource)
            return;

        item = new QStandardItem();
        item->setText(resource->getName());
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setData(resourceId, ResourceIdRole);
        item->setData(0, EventCountRole);
        item->setData(getEventTime(businessAction->getRuntimeParams()), EventTimeRole);
        item->setData(eventReason, EventReasonRole);
        root->appendRow(item);
    }

    item->removeRows(0, item->rowCount());

    item->appendRow(new QStandardItem(eventReason));

    int count = item->data(EventCountRole).toInt() + qMax(businessAction->getAggregationCount(), 1);
    item->setData(count, EventCountRole);
    QString timeStamp = item->data(EventTimeRole).toString();
    if (count == 1)
        item->appendRow(new QStandardItem(tr("at %1").arg(timeStamp)));
    else
        item->appendRow(new QStandardItem(tr("%1 times since %2").arg(count).arg(timeStamp)));
}

void QnPopupWidget::updateConflictTree(const QnAbstractBusinessActionPtr &businessAction) {
    int resourceId = QnBusinessEventRuntime::getEventResourceId(businessAction->getRuntimeParams());

    QStandardItem *root = m_model->invisibleRootItem();
    QStandardItem *item = NULL;

    for (int i = 0; i < root->rowCount(); i++) {
        QStandardItem* child = root->child(i);
        if (child->data(ResourceIdRole).toInt() != resourceId)
            continue;

        item = child;
        break;
    }
    if (item == NULL) {
        QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
        if (!resource)
            return;

        item = new QStandardItem();
        item->setText(resource->getName());
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setData(resourceId, ResourceIdRole);
        item->setData(0, EventCountRole);
        item->setData(getEventTime(businessAction->getRuntimeParams()), EventTimeRole);
        item->setData(QVariant::fromValue(QStringList()), ConflictsRole);
        root->appendRow(item);
    }

    QString source = QnBusinessEventRuntime::getSource(businessAction->getRuntimeParams());
    QStringList newConflicts = QnBusinessEventRuntime::getConflicts(businessAction->getRuntimeParams());

    QStringList conflicts = item->data(ConflictsRole).value<QStringList>();
    foreach(QString entity, newConflicts) {
        if (conflicts.contains(entity, Qt::CaseInsensitive))
            continue;
        conflicts << entity;
    }

    item->removeRows(0, item->rowCount());

    item->appendRow(new QStandardItem(source));
    item->appendRow(new QStandardItem(tr("conflicted with")));
    foreach(QString entity, conflicts)
        item->appendRow(new QStandardItem(entity));

    int count = item->data(EventCountRole).toInt() + qMax(businessAction->getAggregationCount(), 1);
    item->setData(count, EventCountRole);
    QString timeStamp = item->data(EventTimeRole).toString();
    if (count == 1)
        item->appendRow(new QStandardItem(tr("at %1").arg(timeStamp)));
    else
        item->appendRow(new QStandardItem(tr("%1 times since %2").arg(count).arg(timeStamp)));
}
