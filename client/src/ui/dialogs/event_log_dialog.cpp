#include "event_log_dialog.h"
#include "ui_event_log_dialog.h"
#include "ui/models/event_log_model.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "ui/workbench/workbench_context.h"
#include "business/events/abstract_business_event.h"
#include "ui/actions/action_manager.h"
#include "ui/actions/actions.h"
#include "ui/style/resource_icon_cache.h"
#include "device_plugins/server_camera/server_camera.h"
#include "resource_selection_dialog.h"
#include "client/client_globals.h"

QStandardItem* QnEventLogDialog::createEventItem(BusinessEventType::Value value)
{
    QStandardItem* result = new QStandardItem(BusinessEventType::toString(value));
    result->setData((int) value, Qn::FirstItemDataRole);
    return result;
}

QStandardItem* QnEventLogDialog::createEventTree(QStandardItem* rootItem, BusinessEventType::Value value)
{
    QStandardItem* item = createEventItem(value);
    if (rootItem)
        rootItem->appendRow(item);

    foreach(BusinessEventType::Value value, BusinessEventType::childEvents(value))
        createEventTree(item, value);
    return item;
}


QnEventLogDialog::QnEventLogDialog(QWidget *parent, QnWorkbenchContext *context):
    QDialog(parent),
    ui(new Ui::EventLogDialog),
    m_context(context)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    QList<QnEventLogModel::Column> columns;
        columns << QnEventLogModel::DateTimeColumn << QnEventLogModel::EventColumn << QnEventLogModel::EventCameraColumn <<
        QnEventLogModel::ActionColumn << QnEventLogModel::ActionCameraColumn << QnEventLogModel::DescriptionColumn;

    m_model = new QnEventLogModel(this);
    m_model->setColumns(columns);
    ui->gridEvents->setModel(m_model);

    QDate dt = QDateTime::currentDateTime().date();
    ui->dateEditFrom->setDate(dt);
    ui->dateEditTo->setDate(dt);

    ui->gridEvents->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);



    QStandardItem* rootItem = createEventTree(0, BusinessEventType::AnyBusinessEvent);
    
    QStandardItemModel* model = new QStandardItemModel();
    model->appendRow(rootItem);
    ui->eventComboBox->setModel(model);

    QStringList actionItems;
    actionItems << tr("All actions");
    for (int i = 0; i < (int) BusinessActionType::NotDefined; ++i)
        actionItems << BusinessActionType::toString(BusinessActionType::Value(i));
    ui->actionComboBox->addItems(actionItems);

    ui->cameraButton->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera | QnResourceIconCache::Online));

    connect(ui->dateEditFrom, SIGNAL(dateChanged(const QDate &)), this, SLOT(updateData()) );
    connect(ui->dateEditTo, SIGNAL(dateChanged(const QDate &)), this, SLOT(updateData()) );

    connect(ui->eventComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateData()) );
    connect(ui->actionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateData()) );

    //connect(ui->gridEvents->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)), this, SLOT(at_gridCelLSelected(const QModelIndex&)) );

    connect(ui->gridEvents, SIGNAL(customContextMenuRequested (const QPoint &)), this, SLOT(at_customContextMenuRequested(const QPoint &)) );

    connect(ui->cameraButton, SIGNAL(clicked (bool)), this, SLOT(at_cameraButtonClicked()) );

    updateData();
}

QnEventLogDialog::~QnEventLogDialog()
{
}

void QnEventLogDialog::updateData()
{
    BusinessActionType::Value actionType = BusinessActionType::NotDefined;
    BusinessEventType::Value eventType = BusinessEventType::NotDefined;

    QModelIndex idx = ui->eventComboBox->currentIndex();
    if (idx.isValid())
        eventType = (BusinessEventType::Value) ui->eventComboBox->model()->data(idx, Qn::FirstItemDataRole).toInt();

    if (ui->actionComboBox->currentIndex() > 0)
        actionType = BusinessActionType::Value(ui->actionComboBox->currentIndex()-1);

    query(ui->dateEditFrom->dateTime().toMSecsSinceEpoch(), ui->dateEditTo->dateTime().addDays(1).toMSecsSinceEpoch(),
          m_filterCameraList,
          eventType, actionType,
          QnId() // todo: add businessRuleID here
          );

    ui->gridEvents->setDisabled(true);
    ui->groupBox->setCursor(Qt::WaitCursor);
}

QList<QnMediaServerResourcePtr> QnEventLogDialog::getServerList() const
{
    QList<QnMediaServerResourcePtr> result;
    QnResourceList resList = m_context->resourcePool()->getAllResourceByTypeName(lit("Server"));
    foreach(const QnResourcePtr& r, resList) {
        QnMediaServerResourcePtr mServer = r.dynamicCast<QnMediaServerResource>();
        if (mServer)
            result << mServer;
    }

    return result;
}

void QnEventLogDialog::query(qint64 fromMsec, qint64 toMsec, 
                             QnResourceList camList, 
                             BusinessEventType::Value eventType,
                             BusinessActionType::Value actionType,
                             QnId businessRuleId)
{
    m_requests.clear();
    m_allEvents.clear();
    

    QList<QnMediaServerResourcePtr> mediaServerList = getServerList();
    foreach(const QnMediaServerResourcePtr& mserver, mediaServerList)
    {
        if (mserver->getStatus() == QnResource::Online) 
        {
            m_requests << mserver->apiConnection()->asyncEventLog(
                fromMsec, toMsec, 
                m_filterCameraList, 
                eventType,
                actionType,
                businessRuleId, 
                this, SLOT(at_gotEvents(int, const QnLightBusinessActionVectorPtr&, int)));
        }
    }

}

void QnEventLogDialog::at_gotEvents(int httpStatus, const QnLightBusinessActionVectorPtr& events, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (!events->empty())
        m_allEvents << events;
    if (m_requests.isEmpty()) 
    {
        m_model->setEvents(m_allEvents);
        m_allEvents.clear();
        ui->gridEvents->setDisabled(false);
        ui->groupBox->setCursor(Qt::ArrowCursor);
    }
}

void QnEventLogDialog::onItemClicked(QListWidgetItem * item)
{
}

QAction* QnEventLogDialog::getFilterAction(const QMenu* menu, const QModelIndex& idx)
{
    return 0;
}

void QnEventLogDialog::at_customContextMenuRequested(const QPoint& screenPos)
{
    QModelIndex idx = ui->gridEvents->currentIndex();
    QnId resId = m_model->data(idx, Qn::ResourceRole).toInt();
    QnResourcePtr resource = qnResPool->getResourceById(resId);

    QnActionManager *manager = m_context->menu();
    QScopedPointer<QMenu> menu(resource ? manager->newMenu(Qn::ActionScope::TreeScope, QnActionParameters(resource)) : new QMenu);
    //manager->redirectAction(menu.data(), Qn::RenameAction, NULL);

    QAction* filterAction = getFilterAction(menu.data(), idx);
    if (filterAction) {
        if (!menu->isEmpty()) 
            menu->addSeparator();
        menu->addAction(filterAction);
    }

    QAction* action = menu->exec(screenPos + pos());
}

QString QnEventLogDialog::getTextForNCameras(int n) const
{
    if (n == 0)
        return tr("< Any camera >");
    else if (n == 1)
        return tr("< 1 camera >");
    else 
        return tr("< %1 cameras >").arg(n);
}

void QnEventLogDialog::at_cameraButtonClicked()
{
    QnResourceSelectionDialog dialog(this);
    dialog.setSelectedResources(m_filterCameraList);

    if (dialog.exec() == QDialog::Accepted) {
        m_filterCameraList = dialog.getSelectedResources();
        ui->cameraButton->setText(getTextForNCameras(m_filterCameraList.size()));
        updateData();
    }
}
