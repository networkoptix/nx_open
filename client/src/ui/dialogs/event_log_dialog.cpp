
#include "event_log_dialog.h"

#include <QClipboard>
#include <QMenu>
#include <QtGui/QMessageBox>
#include <QMimeData>

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
#include "ui/style/skin.h"
#include "client/client_settings.h"
#include "ui/models/business_rules_actual_model.h"

QnEventLogDialog::QnEventLogDialog(QWidget *parent, QnWorkbenchContext *context):
    QDialog(parent),
    ui(new Ui::EventLogDialog),
    m_context(context),
    m_updateDisabled(false),
    m_dirty(false)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    m_rulesModel = new QnBusinessRulesActualModel(this);

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

    /*
    for (int i = 0; i < (int) QnEventLogModel::ActionCameraColumn; ++i)
        headers->setResizeMode(i, QHeaderView::Fixed);
    for (int i = (int) QnEventLogModel::ActionCameraColumn; i < columns.size(); ++i)
        headers->setResizeMode(i, QHeaderView::ResizeToContents);
    */
    headers->setResizeMode(QHeaderView::Fixed);

    QStandardItem* rootItem = createEventTree(0, BusinessEventType::AnyBusinessEvent);
    QStandardItemModel* model = new QStandardItemModel();
    model->appendRow(rootItem);
    ui->eventComboBox->setModel(model);

    QStringList actionItems;
    actionItems << tr("Any action");
    for (int i = 0; i < (int) BusinessActionType::NotDefined; ++i)
        actionItems << BusinessActionType::toString(BusinessActionType::Value(i));
    ui->actionComboBox->addItems(actionItems);

    m_filterAction      = new QAction(tr("Filter similar rows"), this);
    m_filterAction->setShortcut(Qt::CTRL + Qt::Key_F);
    m_clipboardAction   = new QAction(tr("Copy selection to clipboard"), this);
    m_clipboardAction->setShortcut(QKeySequence::Copy);
    m_resetFilterAction = new QAction(tr("Clear filter"), this);
    m_resetFilterAction->setShortcut(Qt::CTRL + Qt::Key_R);

    ui->gridEvents->addAction(m_clipboardAction);
    ui->gridEvents->addAction(m_filterAction);
    ui->gridEvents->addAction(m_resetFilterAction);

    ui->clearFilterButton->setDefaultAction(m_resetFilterAction);
    ui->cameraButton->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera | QnResourceIconCache::Online));
    ui->refreshButton->setIcon(qnSkin->icon("refresh.png"));
    ui->eventRulesButton->setIcon(qnSkin->icon("tree/layout.png"));

    connect(m_filterAction,         SIGNAL(triggered()),                this, SLOT(at_filterAction()));
    connect(m_resetFilterAction,    SIGNAL(triggered()),                this, SLOT(at_resetFilterAction()));
    connect(m_clipboardAction,      SIGNAL(triggered()),                this, SLOT(at_copyToClipboard()));

    connect(ui->dateEditFrom,       SIGNAL(dateChanged(const QDate&)),  this, SLOT(updateData()) );
    connect(ui->dateEditTo,         SIGNAL(dateChanged(const QDate&)),  this, SLOT(updateData()) );
    connect(ui->eventComboBox,      SIGNAL(currentIndexChanged(int)),   this, SLOT(updateData()) );
    connect(ui->actionComboBox,     SIGNAL(currentIndexChanged(int)),   this, SLOT(updateData()) );
    connect(ui->refreshButton,      SIGNAL(clicked(bool)),              this, SLOT(updateData()) );
    connect(ui->eventRulesButton,   SIGNAL(clicked(bool)),              m_context->action(Qn::BusinessEventsAction), SIGNAL(triggered()));

    connect(ui->cameraButton,       SIGNAL(clicked(bool)),              this, SLOT(at_cameraButtonClicked()) );
    connect(ui->gridEvents,         SIGNAL(clicked(const QModelIndex&)),this, SLOT(at_itemClicked(const QModelIndex&)) );
    connect(ui->gridEvents,         SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(at_customContextMenuRequested(const QPoint&)) );
    connect(qnSettings->notifier(QnClientSettings::IP_SHOWN_IN_TREE), SIGNAL(valueChanged(int)), m_model, SLOT(rebuild()) );
    
    ui->mainGridLayout->activate();
    updateHeaderWidth();

    m_rulesModel->reloadData();
}

QnEventLogDialog::~QnEventLogDialog()
{
}

QStandardItem* QnEventLogDialog::createEventTree(QStandardItem* rootItem, BusinessEventType::Value value)
{
    QStandardItem* item = new QStandardItem(BusinessEventType::toString(value));
    item->setData((int) value, Qn::FirstItemDataRole);

    if (rootItem)
        rootItem->appendRow(item);

    foreach(BusinessEventType::Value value, BusinessEventType::childEvents(value))
        createEventTree(item, value);
    return item;
}

bool QnEventLogDialog::isFilterExist() const
{
    if (!m_filterCameraList.isEmpty())
        return true;
    QModelIndex idx = ui->eventComboBox->currentIndex();
    if (idx.isValid()) {
        BusinessEventType::Value eventType = (BusinessEventType::Value) ui->eventComboBox->model()->data(idx, Qn::FirstItemDataRole).toInt();
        if (eventType != BusinessEventType::NotDefined && eventType != BusinessEventType::AnyBusinessEvent)
            return true;
    }

    if (ui->actionComboBox->currentIndex() > 0)
        return true;

    return false;
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

    // update UI

    m_resetFilterAction->setEnabled(isFilterExist());
    if (m_resetFilterAction->isEnabled())
        m_resetFilterAction->setIcon(qnSkin->icon("tree/clear_hovered.png"));
    else
        m_resetFilterAction->setIcon(qnSkin->icon("tree/clear.png"));

    if (!m_requests.isEmpty()) {
        ui->gridEvents->setDisabled(true);
        setCursor(Qt::BusyCursor);
    }
    else {
        requestFinished(); // just clear grid
        QMessageBox::warning(this, tr("No online media servers"), tr("All media server(s) is offline. No data is selected"));
    }

    ui->dateEditFrom->setDateRange(QDate(2000,1,1), ui->dateEditTo->date());
    ui->dateEditTo->setDateRange(ui->dateEditFrom->date(), QDateTime::currentDateTime().date());
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

void QnEventLogDialog::updateHeaderWidth()
{
    int space = ui->mainGridLayout->horizontalSpacing();
    int offset = ui->gridEvents->verticalHeader()->sizeHint().width();
    space--; // grid line delimiter
    ui->gridEvents->horizontalHeader()->resizeSection(0, ui->dateEditFrom->width() + ui->dateEditTo->width() + ui->delimLabel->width() + space - offset);
    ui->gridEvents->horizontalHeader()->resizeSection(1, ui->eventComboBox->width() + space);
    ui->gridEvents->horizontalHeader()->resizeSection(2, ui->cameraButton->width() + space);
    ui->gridEvents->horizontalHeader()->resizeSection(3, ui->actionComboBox->width() + space);
    ui->gridEvents->horizontalHeader()->resizeSection(4, ui->cameraButton->width() + space);
}

void QnEventLogDialog::at_gotEvents(int httpStatus, const QnLightBusinessActionVectorPtr& events, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (httpStatus == 0 && !events->empty())
        m_allEvents << events;
    if (m_requests.isEmpty()) {
        requestFinished();
        if (m_model->rowCount() == 0 && isFilterExist() && !isRuleExistByCond()) 
        {
            QMessageBox::information(this, tr("No rule(s) for current filter"), tr("You have not configured business rules to match current filter condition."));
        }
    }
}

bool QnEventLogDialog::isCameraMatched(QnBusinessRuleViewModel* ruleModel) const
{
    if (m_filterCameraList.isEmpty())
        return true;
    BusinessEventType::Value eventType = ruleModel->eventType();
    if (!BusinessEventType::requiresCameraResource(eventType))
        return false;
    if (ruleModel->eventResources().isEmpty())
        return true;

    for (int i = 0; i < m_filterCameraList.size(); ++i)
    {
        for (int j = 0; j < ruleModel->eventResources().size(); ++j)
        {
            if (m_filterCameraList[i]->getId() == ruleModel->eventResources()[j]->getId())
                return true;
        }
    }

    return false;
}

bool QnEventLogDialog::isRuleExistByCond() const
{
    if (!m_rulesModel->isLoaded())
        return true;

    QModelIndex idx = ui->eventComboBox->currentIndex();
    BusinessEventType::Value eventType = (BusinessEventType::Value) ui->eventComboBox->model()->data(idx, Qn::FirstItemDataRole).toInt();

    BusinessActionType::Value actionType = BusinessActionType::NotDefined;
    if (ui->actionComboBox->currentIndex() > 0)
        actionType = BusinessActionType::Value(ui->actionComboBox->currentIndex()-1);

    
    for (int i = 0; i < m_rulesModel->rowCount(); ++i)
    {
        QnBusinessRuleViewModel* ruleModel = m_rulesModel->getRuleModel(i);
        if (ruleModel->disabled())
            continue;
        bool isEventMatch = eventType == BusinessEventType::NotDefined || 
                            eventType == BusinessEventType::AnyBusinessEvent ||
                            ruleModel->eventType() == eventType ||
                            BusinessEventType::parentEvent(ruleModel->eventType()) == eventType;
        if (isEventMatch && isCameraMatched(ruleModel))
        {
            if (actionType == BusinessActionType::NotDefined || actionType == ruleModel->actionType())
                return true;
        }
    }
    
    return false;
}

void QnEventLogDialog::requestFinished()
{
    m_model->setEvents(m_allEvents);
    m_allEvents.clear();
    ui->gridEvents->setDisabled(false);
    setCursor(Qt::ArrowCursor);
    updateHeaderWidth();
    if (ui->dateEditFrom->dateTime() != ui->dateEditTo->dateTime())
        setWindowTitle(tr("Event log for period from %1 to %2 - %3 event(s) found")
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate))
        .arg(ui->dateEditTo->dateTime().date().toString(Qt::SystemLocaleLongDate))
        .arg(m_model->rowCount()));
    else
        setWindowTitle(tr("Event log for %1  - %2 event(s) found")
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate))
        .arg(m_model->rowCount()));
}

void QnEventLogDialog::at_itemClicked(const QModelIndex& idx)
{
    if (m_model->hasMotionUrl(idx))
    {
        QnResourcePtr resource = m_model->eventResource(idx.row());
        qint64 pos = m_model->eventTimestamp(idx.row())/1000;
        QnActionParameters params(resource);
        params.setArgument(Qn::ItemTimeRole, pos);

        m_context->menu()->trigger(Qn::OpenInNewLayoutAction, params);
        
        if (isMaximized())
            showNormal();
    }
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

    BusinessEventType::Value eventType = m_model->eventType(idx.row());
    BusinessEventType::Value parentEventType = BusinessEventType::parentEvent(eventType);
    if (parentEventType != BusinessEventType::AnyBusinessEvent && parentEventType != BusinessEventType::NotDefined)
        eventType = parentEventType;
    
    QnSecurityCamResourcePtr cameraResource = m_model->eventResource(idx.row()).dynamicCast<QnSecurityCamResource>();
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
    QMenu* menu = 0;
    QModelIndex idx = ui->gridEvents->currentIndex();
    if (idx.isValid()) 
    {
        QnResourcePtr resource = m_model->getResource(idx);
        QnActionManager *manager = m_context->menu();
        if (resource)
            menu = manager->newMenu(Qn::TreeScope, QnActionParameters(resource));
    }
    if (menu)
        menu->addSeparator();
    else
        menu = new QMenu();

    m_filterAction->setEnabled(idx.isValid());
    m_clipboardAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());

    menu->addAction(m_filterAction);
    menu->addAction(m_resetFilterAction);

    menu->addSeparator();

    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void QnEventLogDialog::at_copyToClipboard()
{
    QAbstractItemModel *model = ui->gridEvents->model();
    QModelIndexList list = ui->gridEvents->selectionModel()->selectedIndexes();
    if(list.isEmpty())
        return;

    qSort(list);

    QString textData;
    QString htmlData;
    QMimeData* mimeData = new QMimeData();

    htmlData.append(lit("<html>\n"));
    htmlData.append(lit("<body>\n"));
    htmlData.append(lit("<table>\n"));

    htmlData.append(lit("<tr>"));
    for(int i = 0; i < list.size() && list[i].row() == list[0].row(); ++i)
    {
        if (i > 0)
            textData.append(lit('\t'));
        QString header = model->headerData(list[i].column(), Qt::Horizontal).toString();
        htmlData.append(lit("<th>"));
        htmlData.append(header);
        htmlData.append(lit("</th>"));
        textData.append(header);
    }
    htmlData.append(lit("</tr>"));

    int prevRow = -1;
    for(int i = 0; i < list.size(); ++i)
    {
        if(list[i].row() != prevRow) {
            prevRow = list[i].row();
            textData.append(lit('\n'));
            if (i > 0)
                htmlData.append(lit("</tr>"));
            htmlData.append(lit("<tr>"));
        }
        else {
            textData.append(lit('\t'));
        }

        htmlData.append(lit("<td>"));
        htmlData.append(model->data(list[i], Qn::DisplayHtmlRole).toString());
        htmlData.append(lit("</td>"));

        textData.append(model->data(list[i]).toString());
    }
    htmlData.append(lit("</tr>\n"));
    htmlData.append(lit("</table>\n"));
    htmlData.append(lit("</body>\n"));
    htmlData.append(lit("</html>\n"));
    textData.append(lit('\n'));

    mimeData->setText(textData);
    mimeData->setHtml(htmlData);

    QApplication::clipboard()->setMimeData(mimeData);
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
        if (isVisible())
            updateData();
    }
}

void QnEventLogDialog::setVisible(bool value)
{
    if (value && !isVisible())
        updateData();
    QDialog::setVisible(value);
}
