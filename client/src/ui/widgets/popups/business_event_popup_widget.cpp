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
#include "business/business_strings_helper.h"

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
    m_eventType(BusinessEventType::NotDefined),
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
    if (m_eventType == BusinessEventType::NotDefined) //not initialized
        initWidget(businessAction->getRuntimeParams().getEventType());
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
    case BusinessEventType::NotDefined:
        return;

    case BusinessEventType::Camera_Input:
    case BusinessEventType::Camera_Disconnect:
        setBorderColor(qnGlobals->popupFrameImportant());
        ui->importantLabel->setVisible(true);
        break;

    case BusinessEventType::Storage_Failure:
    case BusinessEventType::Network_Issue:
    case BusinessEventType::Camera_Ip_Conflict:
    case BusinessEventType::MediaServer_Failure:
    case BusinessEventType::MediaServer_Conflict:
        setBorderColor(qnGlobals->popupFrameWarning());
        ui->warningLabel->setVisible(true);
        break;

        //case BusinessEventType::Camera_Motion:
        //case BusinessEventType::UserDefined:
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
        case BusinessEventType::Camera_Disconnect:
        case BusinessEventType::Camera_Input:
        case BusinessEventType::Camera_Motion:
            item = updateSimpleTree(businessAction->getRuntimeParams());
            break;
        case BusinessEventType::Storage_Failure:
        case BusinessEventType::Network_Issue:
        case BusinessEventType::MediaServer_Failure:
            item = updateReasonTree(businessAction);
            break;
        case BusinessEventType::Camera_Ip_Conflict:
        case BusinessEventType::MediaServer_Conflict:
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

QString QnBusinessEventPopupWidget::getEventTime(const QnBusinessEventParameters &eventParams) {
    qint64 eventTimestamp = eventParams.getEventTimestamp();
    if (eventTimestamp == 0)
        eventTimestamp = qnSyncTime->currentUSecsSinceEpoch();

    return QDateTime::fromMSecsSinceEpoch(eventTimestamp/1000).toString(QLatin1String("hh:mm:ss"));
}

QStandardItem* QnBusinessEventPopupWidget::findOrCreateItem(const QnBusinessEventParameters& eventParams) {
    int resourceId = eventParams.getEventResourceId();
    int eventReasonCode = eventParams.getReasonCode();

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
    item->setData(eventParams.getEventResourceId(), ResourceIdRole);
    item->setData(0, EventCountRole);
    item->setData(getEventTime(eventParams), EventTimeRole);
    item->setData(eventReasonCode, EventReasonRole);
    item->setData(eventParams.getReasonText(), EventReasonTextRole);
    root->appendRow(item);
    return item;
}

QStandardItem* QnBusinessEventPopupWidget::updateSimpleTree(const QnBusinessEventParameters& eventParams) {
    QStandardItem *item = findOrCreateItem(eventParams);
    if (!item)
        return NULL;

    item->removeRows(0, item->rowCount());
    return item;
}

QStandardItem* QnBusinessEventPopupWidget::updateReasonTree(const QnAbstractBusinessActionPtr &businessAction) {
    QStandardItem *item = findOrCreateItem(businessAction->getRuntimeParams());
    if (item) {
        item->removeRows(0, item->rowCount()); //TODO: #GDM fix removing rows
        QString reason = QnBusinessStringsHelper::eventReason(businessAction.data()->getRuntimeParams());
        if (!reason.isEmpty())
            item->appendRow(new QStandardItem(reason));
    }
    return item;
}

QStandardItem* QnBusinessEventPopupWidget::updateConflictTree(const QnBusinessEventParameters& eventParams) {

    QStandardItem *item = findOrCreateItem(eventParams);
    if (!item)
        return NULL;

    QString source = eventParams.getSource();
    QStringList newConflicts = eventParams.getConflicts();

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
