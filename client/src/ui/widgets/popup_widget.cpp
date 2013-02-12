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


    QString extractHost(const QString &url) {
        /* Try it as a host address first. */
        QHostAddress hostAddress(url);
        if(!hostAddress.isNull())
            return hostAddress.toString();

        /* Then go default QUrl route. */
        return QUrl(url).host();
    }

    QString getResourceName(const QnResourcePtr& resource) {
        QnResource::Flags flags = resource->flags();
        if (qnSettings->isIpShownInTree()) {
            if((flags & QnResource::network) || (flags & QnResource::server && flags & QnResource::remote)) {
                QString host = extractHost(resource->getUrl());
                if(!host.isEmpty())
                    return QString(QLatin1String("%1 (%2)")).arg(resource->getName()).arg(host);
            }
        }
        return resource->getName();
    }
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

bool QnPopupWidget::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    if (m_eventType == BusinessEventType::BE_NotDefined) //not initialized
        initWidget(QnBusinessEventRuntime::getEventType(businessAction->getRuntimeParams()));
    bool result = updateTreeModel(businessAction);
    ui->eventsTreeView->expandAll();
    return result;
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

bool QnPopupWidget::updateTreeModel(const QnAbstractBusinessActionPtr &businessAction) {
    QStandardItem* item;
    switch (m_eventType) {
        case BusinessEventType::BE_Camera_Disconnect:
        case BusinessEventType::BE_Camera_Input:
        case BusinessEventType::BE_Camera_Motion:
            item = updateSimpleTree(businessAction->getRuntimeParams());
            break;
        case BusinessEventType::BE_Storage_Failure:
        case BusinessEventType::BE_Network_Issue:
        case BusinessEventType::BE_MediaServer_Failure:
            item = updateReasonTree(businessAction->getRuntimeParams());
            break;
        case BusinessEventType::BE_Camera_Ip_Conflict:
        case BusinessEventType::BE_MediaServer_Conflict:
            item = updateConflictTree(businessAction->getRuntimeParams());
            break;
        default:
            return false;
    }

    if (!item)
        return false;

    int count = item->data(EventCountRole).toInt() + qMax(businessAction->getAggregationCount(), 1);
    item->setData(count, EventCountRole);
    QString timeStamp = item->data(EventTimeRole).toString();
    if (count == 1)
        item->appendRow(new QStandardItem(tr("at %1").arg(timeStamp)));
    else
        item->appendRow(new QStandardItem(tr("%1 times since %2").arg(count).arg(timeStamp)));
    return true;
}

QString QnPopupWidget::getEventTime(const QnBusinessParams &eventParams) {
    qint64 eventTimestamp = QnBusinessEventRuntime::getEventTimestamp(eventParams);
    if (eventTimestamp == 0)
        eventTimestamp = qnSyncTime->currentUSecsSinceEpoch();

    return QDateTime::fromMSecsSinceEpoch(eventTimestamp/1000).toString(QLatin1String("hh:mm:ss"));
}

QStandardItem* QnPopupWidget::findOrCreateItem(const QnBusinessParams& eventParams) {
    int resourceId = QnBusinessEventRuntime::getEventResourceId(eventParams);
    QString eventReason = QnBusinessEventRuntime::getEventReason(eventParams);

    QStandardItem *root = m_model->invisibleRootItem();
    for (int i = 0; i < root->rowCount(); i++) {
        QStandardItem* item = root->child(i);
        if (item->data(ResourceIdRole).toInt() != resourceId)
            continue;
        if (eventReason != QString() && item->data(EventReasonRole).toString() != eventReason)
            continue;

        return item;
    }

    QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
    if (!resource)
        return NULL;

    QStandardItem *item = new QStandardItem();
    item->setText(getResourceName(resource));
    item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
    item->setData(QnBusinessEventRuntime::getEventResourceId(eventParams), ResourceIdRole);
    item->setData(0, EventCountRole);
    item->setData(getEventTime(eventParams), EventTimeRole);
    item->setData(eventReason, EventReasonRole);
    root->appendRow(item);
    return item;
}

QStandardItem* QnPopupWidget::updateSimpleTree(const QnBusinessParams& eventParams) {
    QStandardItem *item = findOrCreateItem(eventParams);
    if (!item)
        return NULL;

    item->removeRows(0, item->rowCount());
    return item;
}

QStandardItem* QnPopupWidget::updateReasonTree(const QnBusinessParams& eventParams) {
    QStandardItem *item = findOrCreateItem(eventParams);
    if (!item)
        return NULL;

    item->removeRows(0, item->rowCount());
    item->appendRow(new QStandardItem(item->data(EventReasonRole).toString()));
    return item;
}

QStandardItem* QnPopupWidget::updateConflictTree(const QnBusinessParams& eventParams) {

    QStandardItem *item = findOrCreateItem(eventParams);
    if (!item)
        return NULL;

    QString source = QnBusinessEventRuntime::getSource(eventParams);
    QStringList newConflicts = QnBusinessEventRuntime::getConflicts(eventParams);

    QStringList conflicts = item->data(ConflictsRole).value<QStringList>();
    foreach(QString entity, newConflicts) {
        if (conflicts.contains(entity, Qt::CaseInsensitive))
            continue;
        conflicts << entity;
    }
    item->setData(QVariant::fromValue(conflicts), ConflictsRole);

    item->removeRows(0, item->rowCount());

    item->appendRow(new QStandardItem(source));
    item->appendRow(new QStandardItem(tr("conflicted with")));
    foreach(QString entity, conflicts)
        item->appendRow(new QStandardItem(entity));
    return item;
}
