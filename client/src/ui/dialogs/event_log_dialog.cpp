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
    m_context(context),
    m_updateDisabled(false),
    m_dirty(false)
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


    QHeaderView* headers = ui->gridEvents->horizontalHeader();
    for (int i = 0; i < 4; ++i)
        headers->setResizeMode(i, QHeaderView::Fixed);
    for (int i = 4; i < columns.size(); ++i)
        headers->setResizeMode(i, QHeaderView::ResizeToContents);

    connect(ui->dateEditFrom, SIGNAL(resizeEvent(QResizeEvent*)), this, SLOT(at_ControlResized()));
    connect(ui->eventComboBox, SIGNAL(resizeEvent(QResizeEvent*)), this, SLOT(at_ControlResized()));
    connect(ui->cameraButton, SIGNAL(resizeEvent(QResizeEvent*)), this, SLOT(at_ControlResized()));
    connect(ui->actionComboBox, SIGNAL(resizeEvent(QResizeEvent*)), this, SLOT(at_ControlResized()));


    QStandardItem* rootItem = createEventTree(0, BusinessEventType::AnyBusinessEvent);
    QStandardItemModel* model = new QStandardItemModel();
    model->appendRow(rootItem);
    ui->eventComboBox->setModel(model);

    QStringList actionItems;
    actionItems << tr("Any action");
    for (int i = 0; i < (int) BusinessActionType::NotDefined; ++i)
        actionItems << BusinessActionType::toString(BusinessActionType::Value(i));
    ui->actionComboBox->addItems(actionItems);

    ui->cameraButton->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera | QnResourceIconCache::Online));

    connect(ui->dateEditFrom, SIGNAL(dateChanged(const QDate &)), this, SLOT(updateData()) );
    connect(ui->dateEditTo, SIGNAL(dateChanged(const QDate &)), this, SLOT(updateData()) );

    connect(ui->eventComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateData()) );
    connect(ui->actionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateData()) );

    //connect(ui->gridEvents->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)), this, SLOT(at_gridCelLSelected(const QModelIndex&)) );

    connect(ui->gridEvents, SIGNAL(clicked (const QModelIndex &)), this, SLOT(at_itemClicked(const QModelIndex&)) );
    connect(ui->gridEvents, SIGNAL(customContextMenuRequested (const QPoint &)), this, SLOT(at_customContextMenuRequested(const QPoint &)) );
    connect(ui->cameraButton, SIGNAL(clicked (bool)), this, SLOT(at_cameraButtonClicked()) );

    updateData();

    int w = ui->actionComboBox->width();
    int w2 = ui->dateEditFrom->width();
    int w3 = ui->eventComboBox->width();
    int w4 = ui->cameraButton->width();
    w = w;
}

QnEventLogDialog::~QnEventLogDialog()
{
}

void QnEventLogDialog::updateData()
{
    if (m_updateDisabled) {
        m_dirty = true;
        return;
    }

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
    ui->gridEvents->setCursor(Qt::WaitCursor);
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

void QnEventLogDialog::at_ControlResized(QResizeEvent* event)
{
    int colIdx = -1;
    if (sender() == ui->dateEditFrom)
        colIdx = 0;
    else if (sender() == ui->eventComboBox)
        colIdx = 1;
    else if (sender() == ui->cameraButton)
        colIdx = 2;
    else if (sender() == ui->actionComboBox)
        colIdx = 3;
    
    if (colIdx >= 0)
        ui->gridEvents->horizontalHeader()->resizeSection(colIdx, event->size().width());
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
        ui->gridEvents->setCursor(Qt::ArrowCursor);
    }
}

void QnEventLogDialog::at_itemClicked(const QModelIndex& idx)
{
    if (!idx.isValid() || idx.column() != QnEventLogModel::DescriptionColumn)
        return;
    
    QnResourcePtr resource = m_model->eventResource(idx);
    if (m_model->eventType(idx) != BusinessEventType::Camera_Motion || !resource)
        return;
    qint64 pos = m_model->eventTimestamp(idx)/1000;

    QnActionParameters params(resource);
    params.setArgument(Qn::ItemTimeRole, pos);

    m_context->menu()->trigger(Qn::OpenInNewLayoutAction, params);
}

bool QnEventLogDialog::setEventTypeRecursive(BusinessEventType::Value value, QAbstractItemModel* model, const QModelIndex& parentItem)
{
    for (int i = 0; i < model->rowCount(parentItem); ++i)
    {
        QModelIndex idx = model->index(i, 0, parentItem);
        BusinessEventType::Value curVal = (BusinessEventType::Value) model->data(idx, Qn::FirstItemDataRole).toInt();
        if (curVal == value)
        {
            ui->eventComboBox->selectIndex(idx);
            return true;
        }
        if (model->hasChildren(idx))
            setEventTypeRecursive(value, model, idx);
    }
    return false;
}

void QnEventLogDialog::setEventType(BusinessEventType::Value value)
{
    setEventTypeRecursive(value, ui->eventComboBox->model(), QModelIndex());
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

void QnEventLogDialog::setCameraList(QnResourceList resList)
{
    if (resList.size() == m_filterCameraList.size())
    {
        bool matched = true;
        for (int i = 0; i < resList.size(); ++i)
        {
            matched &= resList[i]->getId() == m_filterCameraList[i]->getId();
        }
        if (matched)
            return;
    }

    m_filterCameraList = resList;
    ui->cameraButton->setText(getTextForNCameras(m_filterCameraList.size()));

    updateData();
}

void QnEventLogDialog::setActionType(BusinessActionType::Value value)
{
    if (value == BusinessActionType::NotDefined)
        ui->actionComboBox->setCurrentIndex(0);
    else
        ui->actionComboBox->setCurrentIndex(int(value) - 1);
}

void QnEventLogDialog::at_resetFilterAction()
{
    disableUpdateData();
    setEventType(BusinessEventType::AnyBusinessEvent);
    setCameraList(QnResourceList());
    setActionType(BusinessActionType::NotDefined);
    enableUpdateData();
}

void QnEventLogDialog::at_filterAction()
{
    QModelIndex idx = ui->gridEvents->currentIndex();

    BusinessEventType::Value eventType = m_model->eventType(idx);
    BusinessEventType::Value parentEventType = BusinessEventType::parentEvent(eventType);
    if (parentEventType != BusinessEventType::AnyBusinessEvent && parentEventType != BusinessEventType::NotDefined)
        eventType = parentEventType;
    
    QnSecurityCamResourcePtr cameraResource = m_model->eventResource(idx).dynamicCast<QnSecurityCamResource>();
    QnResourceList camList;
    if (cameraResource)
        camList << cameraResource;

    disableUpdateData();
    setEventType(eventType);
    setCameraList(camList);
    setActionType(BusinessActionType::NotDefined);
    enableUpdateData();
}

void QnEventLogDialog::at_customContextMenuRequested(const QPoint&)
{
    QModelIndex idx = ui->gridEvents->currentIndex();
    if (!idx.isValid())
        return;

    QnId resId = m_model->data(idx, Qn::ResourceRole).toInt();
    QnResourcePtr resource = qnResPool->getResourceById(resId);

    QnActionManager *manager = m_context->menu();
    QScopedPointer<QMenu> menu(resource ? manager->newMenu(Qn::ActionScope::TreeScope, QnActionParameters(resource)) : new QMenu);

    if (!menu->isEmpty())
        menu->addSeparator();

    QAction* filterAction = new QAction(tr("&Filter similar rows"), menu.data());
    connect(filterAction, SIGNAL(triggered()), this, SLOT(at_filterAction()));
    menu->addAction(filterAction);

    QAction* resetFilterAction = new QAction(tr("&Display all rows"), menu.data());
    connect(resetFilterAction, SIGNAL(triggered()), this, SLOT(at_resetFilterAction()));
    menu->addAction(resetFilterAction);

    QAction* action = menu->exec(QCursor::pos());
}

void QnEventLogDialog::at_cameraButtonClicked()
{
    QnResourceSelectionDialog dialog(this);
    dialog.setSelectedResources(m_filterCameraList);

    if (dialog.exec() == QDialog::Accepted)
        setCameraList(dialog.getSelectedResources());
}

void QnEventLogDialog::disableUpdateData()
{
    m_updateDisabled = true;
}

void QnEventLogDialog::enableUpdateData()
{
    m_updateDisabled = false;
    if (m_dirty) {
        m_dirty = false;
        updateData();
    }
}
