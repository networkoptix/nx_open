#include "business_event_popup_widget.h"
#include "ui_business_event_popup_widget.h"

#include <business/events/conflict_business_event.h>
#include <business/events/reasoned_business_event.h>

#include <core/resource/resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/common/resource_name.h>
#include <ui/style/globals.h>
#include <ui/style/resource_icon_cache.h>

#include <utils/common/synctime.h>

namespace {

    enum ItemDataRole {
        ResourceIdRole = Qt::UserRole + 1,
        EventCountRole,
        EventTimeRole,
        EventReasonRole,
        EventReasonTextRole,
        ConflictsRole

    };
}

QnBusinessEventPopupWidget::QnBusinessEventPopupWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnBusinessEventPopupWidget),
    m_eventType(BusinessEventType::BE_NotDefined),
    m_showAllItem(NULL),
    m_showAll(false)
{
    ui->setupUi(this);
    connect(ui->okButton, SIGNAL(clicked()), this, SLOT(at_okButton_clicked()));

    m_model = new QStandardItemModel(this);
    ui->eventsTreeView->setModel(m_model);

    connect(ui->eventsTreeView, SIGNAL(clicked(QModelIndex)), this, SLOT(at_eventsTreeView_clicked(QModelIndex)));
    connect(ui->eventsTreeView, SIGNAL(expanded(QModelIndex)), this, SLOT(updateTreeSize()));
    connect(ui->eventsTreeView, SIGNAL(collapsed(QModelIndex)), this, SLOT(updateTreeSize()));

    QPalette pal = ui->eventsTreeView->palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Window));
    ui->eventsTreeView->setPalette(pal);
}

QnBusinessEventPopupWidget::~QnBusinessEventPopupWidget()
{
}

void QnBusinessEventPopupWidget::at_okButton_clicked() {
    emit closed(m_eventType, ui->ignoreCheckBox->isChecked());
    hide();
}

void QnBusinessEventPopupWidget::at_eventsTreeView_clicked(const QModelIndex &index) {
    if (m_showAll)
        return;

    QStandardItem* item = m_model->itemFromIndex(index);
    if (!m_showAllItem || item != m_showAllItem)
        return;

    m_showAll = true;

    QStandardItem* root = m_model->invisibleRootItem();
    QModelIndex rootIndex = m_model->indexFromItem(root);
    int showAllRow = -1;
    for (int i = 0; i < root->rowCount(); i++) {
        if (root->child(i) == m_showAllItem) {
            showAllRow = i;
            continue;
        }
        ui->eventsTreeView->setRowHidden(i, rootIndex, false);
    }
    if (showAllRow >= 0)
        root->removeRow(showAllRow);

    ui->eventsTreeView->expandAll();
    updateTreeSize();

}

void QnBusinessEventPopupWidget::updateTreeSize() {
    int height = 0;
    QStandardItem* root = m_model->invisibleRootItem();
    QModelIndex rootIndex = m_model->indexFromItem(root);

    for (int i = 0; i < root->rowCount(); i++) {
        QStandardItem* item = root->child(i);
        if (ui->eventsTreeView->isRowHidden(i, rootIndex))
            continue;
        height += ui->eventsTreeView->rowHeight(m_model->indexFromItem(item));
        if (!ui->eventsTreeView->isExpanded(m_model->indexFromItem(item)))
            continue;

        for (int j = 0; j < item->rowCount(); j++) {
            height += ui->eventsTreeView->rowHeight(m_model->indexFromItem(item->child(j)));
        }
    }

    ui->eventsTreeView->setMaximumHeight(height);
}

bool QnBusinessEventPopupWidget::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    if (m_eventType == BusinessEventType::BE_NotDefined) //not initialized
        initWidget(QnBusinessEventRuntime::getEventType(businessAction->getRuntimeParams()));
    if (!updateTreeModel(businessAction))
        return false;

  //  ui->eventsTreeView->expandAll();
    QStandardItem* root = m_model->invisibleRootItem();


    if (!m_showAll && !m_showAllItem && root->rowCount() > 2) {
        //add '...'

        m_showAllItem = new QStandardItem();
        m_showAllItem->setForeground(QBrush(Qt::yellow));

        QFont font = m_showAllItem->font();
        font.setUnderline(true);
        m_showAllItem->setFont(font);
        root->appendRow(m_showAllItem);
    }

    int hidden = 0;
    if (!m_showAll && m_showAllItem) {
        //hide additional rows

        QModelIndex index = m_model->indexFromItem(root);

        for (int i = 2; i < root->rowCount(); i++) {
            if (root->child(i) == m_showAllItem)
                continue;
            ui->eventsTreeView->setRowHidden(i, index, true);
            hidden++;
        }
        m_showAllItem->setText(tr("%1 others...").arg(hidden));
    }

    updateTreeSize();
    return true;
}

void QnBusinessEventPopupWidget::initWidget(BusinessEventType::Value eventType) {
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
        setBorderColor(qnGlobals->popupFrameImportant());
        ui->importantLabel->setVisible(true);
        break;

    case BusinessEventType::BE_Storage_Failure:
    case BusinessEventType::BE_Network_Issue:
    case BusinessEventType::BE_Camera_Ip_Conflict:
    case BusinessEventType::BE_MediaServer_Failure:
    case BusinessEventType::BE_MediaServer_Conflict:
        setBorderColor(qnGlobals->popupFrameWarning());
        ui->warningLabel->setVisible(true);
        break;

        //case BusinessEventType::BE_Camera_Motion:
        //case BusinessEventType::BE_UserDefined:
    default:
        setBorderColor(qnGlobals->popupFrameNotification());
        ui->notificationLabel->setVisible(true);
        break;
    }
    ui->eventLabel->setText(BusinessEventType::toString(eventType));
}

bool QnBusinessEventPopupWidget::updateTreeModel(const QnAbstractBusinessActionPtr &businessAction) {
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

QString QnBusinessEventPopupWidget::getEventTime(const QnBusinessParams &eventParams) {
    qint64 eventTimestamp = QnBusinessEventRuntime::getEventTimestamp(eventParams);
    if (eventTimestamp == 0)
        eventTimestamp = qnSyncTime->currentUSecsSinceEpoch();

    return QDateTime::fromMSecsSinceEpoch(eventTimestamp/1000).toString(QLatin1String("hh:mm:ss"));
}

QStandardItem* QnBusinessEventPopupWidget::findOrCreateItem(const QnBusinessParams& eventParams) {
    int resourceId = QnBusinessEventRuntime::getEventResourceId(eventParams);
    int eventReasonCode = QnBusinessEventRuntime::getReasonCode(eventParams);

    QStandardItem *root = m_model->invisibleRootItem();
    for (int i = 0; i < root->rowCount(); i++) {
        QStandardItem* item = root->child(i);
        if (item->data(ResourceIdRole).toInt() != resourceId)
            continue;
        if (eventReasonCode > 0 && item->data(EventReasonRole).toInt() != eventReasonCode)
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
    item->setData(eventReasonCode, EventReasonRole);
    item->setData(QnBusinessEventRuntime::getReasonText(eventParams), EventReasonTextRole);
    root->appendRow(item);
    return item;
}

QStandardItem* QnBusinessEventPopupWidget::updateSimpleTree(const QnBusinessParams& eventParams) {
    QStandardItem *item = findOrCreateItem(eventParams);
    if (!item)
        return NULL;

    item->removeRows(0, item->rowCount());
    return item;
}

QStandardItem* QnBusinessEventPopupWidget::updateReasonTree(const QnBusinessParams& eventParams) {
    QStandardItem *item = findOrCreateItem(eventParams);
    if (!item)
        return NULL;

    item->removeRows(0, item->rowCount()); //TODO: #GDM fix removing rows

    QnBusiness::EventReason reasonCode = (QnBusiness::EventReason)item->data(EventReasonRole).toInt();
    QString reasonText = item->data(EventReasonTextRole).toString();

    switch (reasonCode) {
        case QnBusiness::NetworkIssueNoFrame:
            if (m_eventType == BusinessEventType::BE_Network_Issue)
                item->appendRow(new QStandardItem(tr("No video frame received\nduring last %1 seconds.")
                                                  .arg(reasonText)));
            break;
        case QnBusiness::NetworkIssueConnectionClosed:
            if (m_eventType == BusinessEventType::BE_Network_Issue)
                item->appendRow(new QStandardItem(tr("Connection to camera\nwas unexpectedly closed")));
            break;
        case QnBusiness::NetworkIssueRtpPacketLoss:
            if (m_eventType == BusinessEventType::BE_Network_Issue) {
                QStringList seqs = reasonText.split(QLatin1Char(';'));
                if (seqs.size() != 2)
                    break;
                QString text = tr("RTP packet loss detected.\nPrev seq.=%1 next seq.=%2")
                        .arg(seqs[0])
                        .arg(seqs[1]);
                item->appendRow(new QStandardItem(text));
            }
            break;
        case QnBusiness::MServerIssueTerminated:
            if (m_eventType == BusinessEventType::BE_MediaServer_Failure)
                item->appendRow(new QStandardItem(tr("Server terminated.")));
            break;
        case QnBusiness::MServerIssueStarted:
            if (m_eventType == BusinessEventType::BE_MediaServer_Failure)
                item->appendRow(new QStandardItem(tr("Server started after crash.")));
            break;
        case QnBusiness::StorageIssueIoError:
            if (m_eventType == BusinessEventType::BE_Storage_Failure)
                item->appendRow(new QStandardItem(tr("I/O Error occured at\n%1")
                                                  .arg(reasonText)));
            break;
        case QnBusiness::StorageIssueNotEnoughSpeed:
            if (m_eventType == BusinessEventType::BE_Storage_Failure)
                item->appendRow(new QStandardItem(tr("Not enough HDD/SSD speed\nfor recording at\n%1.")
                                                  .arg(reasonText)));
            break;
        default:
            break;
    }

    return item;
}

QStandardItem* QnBusinessEventPopupWidget::updateConflictTree(const QnBusinessParams& eventParams) {

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
