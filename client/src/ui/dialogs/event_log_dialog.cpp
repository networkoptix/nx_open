#include "event_log_dialog.h"
#include "ui_event_log_dialog.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <utils/common/event_processors.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <business/events/abstract_business_event.h>
#include <business/business_strings_helper.h>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <device_plugins/server_camera/server_camera.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/actions.h>
#include <ui/common/grid_widget_helper.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/models/business_rule_view_model.h>
#include <ui/models/event_log_model.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>
#include <ui/workbench/workbench_context.h>

namespace {
    const int ProlongedActionRole = Qt::UserRole + 2;
}


QnEventLogDialog::QnEventLogDialog(QWidget *parent, QnWorkbenchContext *context):
    base_type(parent, Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowSystemMenuHint | Qt::WindowContextHelpButtonHint | Qt::WindowCloseButtonHint
#ifdef Q_OS_MAC
    | Qt::Tool
#endif
    ),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::EventLogDialog),
    m_eventTypesModel(new QStandardItemModel()),
    m_actionTypesModel(new QStandardItemModel()),
    m_updateDisabled(false),
    m_dirty(false),
    m_lastMouseButton(Qt::NoButton)
{
    ui->setupUi(this);
    setWarningStyle(ui->warningLabel);

    setHelpTopic(this, Qn::MainWindow_Notifications_EventLog_Help);

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
    headers->setSectionResizeMode(QHeaderView::Fixed);

    // init events model
    {
        QStandardItem* rootItem = createEventTree(0, BusinessEventType::AnyBusinessEvent);
        m_eventTypesModel->appendRow(rootItem);
        ui->eventComboBox->setModel(m_eventTypesModel);
    }

    // init actions model
    {
        QStandardItem *anyActionItem = new QStandardItem(tr("Any action"));
        anyActionItem->setData(BusinessActionType::NotDefined);
        anyActionItem->setData(false, ProlongedActionRole);
        m_actionTypesModel->appendRow(anyActionItem);


        for (int i = 0; i < BusinessActionType::Count; i++) {
            BusinessActionType::Value val = (BusinessActionType::Value)i;

            QStandardItem *item = new QStandardItem(QnBusinessStringsHelper::actionName(val));
            item->setData(val);
            item->setData(BusinessActionType::hasToggleState(val), ProlongedActionRole);

            QList<QStandardItem *> row;
            row << item;
            m_actionTypesModel->appendRow(row);
        }
        ui->actionComboBox->setModel(m_actionTypesModel);
    }

    m_filterAction      = new QAction(tr("Filter Similar Rows"), this);
    m_filterAction->setShortcut(Qt::CTRL + Qt::Key_F);
    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(Qt::CTRL + Qt::Key_A);
    m_clipboardAction->setShortcut(QKeySequence::Copy);
    m_resetFilterAction = new QAction(tr("Clear Filter"), this);
    m_resetFilterAction->setShortcut(Qt::CTRL + Qt::Key_R);

    QnSingleEventSignalizer *mouseSignalizer = new QnSingleEventSignalizer(this);
    mouseSignalizer->setEventType(QEvent::MouseButtonRelease);
    ui->gridEvents->viewport()->installEventFilter(mouseSignalizer);
    connect(mouseSignalizer,       SIGNAL(activated(QObject *, QEvent *)), this, SLOT(at_mouseButtonRelease(QObject *, QEvent *)));

    ui->gridEvents->addAction(m_clipboardAction);
    ui->gridEvents->addAction(m_exportAction);
    ui->gridEvents->addAction(m_filterAction);
    ui->gridEvents->addAction(m_resetFilterAction);

    ui->clearFilterButton->setDefaultAction(m_resetFilterAction);
    ui->cameraButton->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera | QnResourceIconCache::Online));
    ui->refreshButton->setIcon(qnSkin->icon("refresh.png"));
    ui->eventRulesButton->setIcon(qnSkin->icon("tree/layout.png"));
    ui->loadingProgressBar->hide();

    connect(m_filterAction,         SIGNAL(triggered()),                this, SLOT(at_filterAction()));
    connect(m_resetFilterAction,    SIGNAL(triggered()),                this, SLOT(at_resetFilterAction()));
    connect(m_clipboardAction,      SIGNAL(triggered()),                this, SLOT(at_copyToClipboard()));
    connect(m_exportAction,         SIGNAL(triggered()),                this, SLOT(at_exportAction()));
    connect(m_selectAllAction,      SIGNAL(triggered()),                this, SLOT(at_selectAllAction()));

    connect(ui->dateEditFrom,       SIGNAL(dateChanged(const QDate&)),  this, SLOT(updateData()) );
    connect(ui->dateEditTo,         SIGNAL(dateChanged(const QDate&)),  this, SLOT(updateData()) );
    connect(ui->eventComboBox,      SIGNAL(currentIndexChanged(int)),   this, SLOT(updateData()) );
    connect(ui->actionComboBox,     SIGNAL(currentIndexChanged(int)),   this, SLOT(updateData()) );
    connect(ui->refreshButton,      SIGNAL(clicked(bool)),              this, SLOT(updateData()) );
    connect(ui->eventRulesButton,   SIGNAL(clicked(bool)),              this->context()->action(Qn::BusinessEventsAction), SIGNAL(triggered()));

    connect(ui->cameraButton,       SIGNAL(clicked(bool)),              this, SLOT(at_cameraButtonClicked()) );
    connect(ui->gridEvents,         SIGNAL(clicked(const QModelIndex&)),this, SLOT(at_itemClicked(const QModelIndex&)) );
    connect(ui->gridEvents,         SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(at_customContextMenuRequested(const QPoint&)) );
    connect(qnSettings->notifier(QnClientSettings::IP_SHOWN_IN_TREE), SIGNAL(valueChanged(int)), m_model, SLOT(rebuild()) );

    ui->mainGridLayout->activate();
    updateHeaderWidth();
}

QnEventLogDialog::~QnEventLogDialog()
{
}

QStandardItem* QnEventLogDialog::createEventTree(QStandardItem* rootItem, BusinessEventType::Value value)
{
    QStandardItem* item = new QStandardItem(QnBusinessStringsHelper::eventName(value));
    item->setData(value);

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
        BusinessEventType::Value eventType = (BusinessEventType::Value) m_eventTypesModel->itemFromIndex(idx)->data().toInt();
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
    m_updateDisabled = true;

    BusinessEventType::Value eventType = BusinessEventType::NotDefined;
    {
        QModelIndex idx = ui->eventComboBox->currentIndex();
        if (idx.isValid())
            eventType = (BusinessEventType::Value) m_eventTypesModel->itemFromIndex(idx)->data().toInt();

        bool serverIssue = BusinessEventType::parentEvent(eventType) == BusinessEventType::AnyServerIssue || eventType == BusinessEventType::AnyServerIssue;
        ui->cameraButton->setEnabled(!serverIssue);
        if (serverIssue)
            setCameraList(QnResourceList());

        bool istantOnly = !BusinessEventType::hasToggleState(eventType) && eventType != BusinessEventType::NotDefined;
        updateActionList(istantOnly);
    }

    BusinessActionType::Value actionType = BusinessActionType::NotDefined;
    {
        int idx = ui->actionComboBox->currentIndex();
        actionType = (BusinessActionType::Value) m_actionTypesModel->item(idx)->data().toInt();
    }

    query(ui->dateEditFrom->dateTime().toMSecsSinceEpoch(),
          ui->dateEditTo->dateTime().addDays(1).toMSecsSinceEpoch(),
          eventType,
          actionType);

    // update UI

    m_resetFilterAction->setEnabled(isFilterExist());
    if (m_resetFilterAction->isEnabled())
        m_resetFilterAction->setIcon(qnSkin->icon("tree/clear_hovered.png"));
    else
        m_resetFilterAction->setIcon(qnSkin->icon("tree/clear.png"));

    if (!m_requests.isEmpty()) {
        ui->gridEvents->setDisabled(true);
        ui->stackedWidget->setCurrentWidget(ui->gridPage);
        setCursor(Qt::BusyCursor);
        ui->loadingProgressBar->show();
    }
    else {
        requestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    ui->dateEditFrom->setDateRange(QDate(2000,1,1), ui->dateEditTo->date());
    ui->dateEditTo->setDateRange(ui->dateEditFrom->date(), QDateTime::currentDateTime().date());

    m_updateDisabled = false;
    m_dirty = false;
}

QList<QnMediaServerResourcePtr> QnEventLogDialog::getServerList() const
{
    QList<QnMediaServerResourcePtr> result;
    QnResourceList resList = qnResPool->getAllResourceByTypeName(lit("Server"));
    foreach(const QnResourcePtr& r, resList) {
        QnMediaServerResourcePtr mServer = r.dynamicCast<QnMediaServerResource>();
        if (mServer)
            result << mServer;
    }

    return result;
}

void QnEventLogDialog::query(qint64 fromMsec, qint64 toMsec,
                             BusinessEventType::Value eventType,
                             BusinessActionType::Value actionType)
{
    m_requests.clear();
    m_allEvents.clear();


    QList<QnMediaServerResourcePtr> mediaServerList = getServerList();
    foreach(const QnMediaServerResourcePtr& mserver, mediaServerList)
    {
        if (mserver->getStatus() == QnResource::Online)
        {
            m_requests << mserver->apiConnection()->getEventLogAsync(
                fromMsec, toMsec,
                m_filterCameraList,
                eventType,
                actionType,
                QnId(),
                this, SLOT(at_gotEvents(int, const QnBusinessActionDataListPtr&, int)));
        }
    }
}

void QnEventLogDialog::updateHeaderWidth()
{
    if (ui->dateEditFrom->width() == 0)
        return;

    int space = ui->mainGridLayout->horizontalSpacing();
    int offset = 0; // ui->gridEvents->verticalHeader()->sizeHint().width();
    space--; // grid line delimiter
    ui->gridEvents->horizontalHeader()->resizeSection(0, ui->dateEditFrom->width() + ui->dateEditTo->width() + ui->delimLabel->width() + space - offset);
    ui->gridEvents->horizontalHeader()->resizeSection(1, ui->eventComboBox->width() + space);
    ui->gridEvents->horizontalHeader()->resizeSection(2, ui->cameraButton->width() + space);
    ui->gridEvents->horizontalHeader()->resizeSection(3, ui->actionComboBox->width() + space);

    int w = 0;
    QSet<QString> cache;
    QFontMetrics fm(ui->gridEvents->font());
    for (int i = 0; i < m_model->rowCount(); ++i)
    {
        QModelIndex idx = m_model->index(i, (int) QnEventLogModel::ActionCameraColumn);
        QString targetText = m_model->data(idx).toString();
        int spaceIdx = targetText.indexOf(L' ');
        int prevPos = 0;
        if (spaceIdx == -1) {
            cache << targetText;
        }
        else {
            while (spaceIdx >= 0) {
                cache << targetText.mid(prevPos, spaceIdx - prevPos);
                prevPos = spaceIdx;
                spaceIdx = targetText.indexOf(L' ', spaceIdx+1);
            }
            cache << targetText.mid(prevPos, targetText.length() - prevPos);
        }
    }
    
    foreach(const QString& str, cache)
        w = qMax(w, fm.size(0, str).width());

    ui->gridEvents->horizontalHeader()->resizeSection(4, qMax(w + 32, ui->cameraButton->width() + space));
}

void QnEventLogDialog::at_gotEvents(int httpStatus, const QnBusinessActionDataListPtr& events, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (httpStatus == 0 && !events->empty())
        m_allEvents << events;
    if (m_requests.isEmpty()) {
        requestFinished();
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

void QnEventLogDialog::requestFinished()
{
    m_model->setEvents(m_allEvents);
    m_allEvents.clear();
    ui->gridEvents->setDisabled(false);
    setCursor(Qt::ArrowCursor);
    updateHeaderWidth();
    if (ui->dateEditFrom->dateTime() != ui->dateEditTo->dateTime())
        ui->statusLabel->setText(tr("Event log for period from %1 to %2 - %n event(s) found", "", m_model->rowCount())
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate))
        .arg(ui->dateEditTo->dateTime().date().toString(Qt::SystemLocaleLongDate)));
    else
        ui->statusLabel->setText(tr("Event log for %1 - %n event(s) found", "", m_model->rowCount())
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate)));
    ui->loadingProgressBar->hide();
}

void QnEventLogDialog::at_itemClicked(const QModelIndex& idx)
{
    if (m_lastMouseButton == Qt::LeftButton && m_model->hasMotionUrl(idx))
    {
        QnResourcePtr resource = m_model->eventResource(idx.row());
        qint64 pos = m_model->eventTimestamp(idx.row())/1000;
        QnActionParameters params(resource);
        params.setArgument(Qn::ItemTimeRole, pos);

        context()->menu()->trigger(Qn::OpenInNewLayoutAction, params);

        if (isMaximized())
            showNormal();
    }
}

void QnEventLogDialog::setEventType(BusinessEventType::Value value)
{
    QModelIndexList found = m_eventTypesModel->match(
                m_eventTypesModel->index(0, 0),
                Qt::UserRole + 1,
                value,
                1,
                Qt::MatchExactly | Qt::MatchRecursive);
    if (found.isEmpty())
        ui->eventComboBox->setCurrentIndex(QModelIndex());
    else
        ui->eventComboBox->setCurrentIndex(found.first());
}

QString QnEventLogDialog::getTextForNCameras(int n) const
{
    if (n == 0)
        return tr("<Any camera>");
    else 
        return tr("<%n camera(s)>", "", n);
}

void QnEventLogDialog::setDateRange(const QDate& from, const QDate& to)
{
    ui->dateEditFrom->setDateRange(QDate(2000,1,1), to);
    ui->dateEditTo->setDateRange(from, QDateTime::currentDateTime().date());

    ui->dateEditTo->setDate(to);
    ui->dateEditFrom->setDate(from);
}

void QnEventLogDialog::setCameraList(const QnResourceList &cameras)
{
    if (cameras.size() == m_filterCameraList.size())
    {
        bool matched = true;
        for (int i = 0; i < cameras.size(); ++i)
        {
            matched &= cameras[i]->getId() == m_filterCameraList[i]->getId();
        }
        if (matched)
            return;
    }

    m_filterCameraList = cameras;
    ui->cameraButton->setText(getTextForNCameras(m_filterCameraList.size()));

    updateData();
}

void QnEventLogDialog::setActionType(BusinessActionType::Value value)
{
    if (value == BusinessActionType::NotDefined)
        ui->actionComboBox->setCurrentIndex(0);
    else
        ui->actionComboBox->setCurrentIndex(int(value) + 1);
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
        QnResourcePtr resource = m_model->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        QnActionManager *manager = context()->menu();
        if (resource) {
            menu = manager->newMenu(Qn::TreeScope, this, QnActionParameters(resource));
            foreach(QAction* action, menu->actions())
                action->setShortcut(QKeySequence());
        }
    }
    if (menu)
        menu->addSeparator();
    else
        menu = new QMenu(this);

    m_filterAction->setEnabled(idx.isValid());
    m_clipboardAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());
    m_exportAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());

    menu->addAction(m_filterAction);
    menu->addAction(m_resetFilterAction);

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void QnEventLogDialog::at_selectAllAction()
{
    ui->gridEvents->selectAll();
}

void QnEventLogDialog::at_exportAction()
{
    QnGridWidgetHelper(context()).exportToFile(ui->gridEvents, tr("Export selected events to file"));
}

void QnEventLogDialog::at_copyToClipboard()
{
    QnGridWidgetHelper(context()).copyToClipboard(ui->gridEvents);
}

void QnEventLogDialog::at_mouseButtonRelease(QObject* sender, QEvent* event)
{
    Q_UNUSED(sender)
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

void QnEventLogDialog::at_cameraButtonClicked()
{
    QnResourceSelectionDialog dialog(this);
    dialog.setSelectedResources(m_filterCameraList);

    if (dialog.exec() == QDialog::Accepted)
        setCameraList(dialog.selectedResources());
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

void QnEventLogDialog::updateActionList(bool instantOnly)
{
    QModelIndexList prolongedActions = m_actionTypesModel->match(m_actionTypesModel->index(0,0),
                                                                 ProlongedActionRole, true, -1, Qt::MatchExactly);

    // what type of actions to show: prolonged or instant
    bool enableProlongedActions = !instantOnly;
    foreach (QModelIndex idx, prolongedActions) {
        m_actionTypesModel->item(idx.row())->setEnabled(enableProlongedActions);
        m_actionTypesModel->item(idx.row())->setSelectable(enableProlongedActions);
    }

    if (!m_actionTypesModel->item(ui->actionComboBox->currentIndex())->isEnabled())
        ui->actionComboBox->setCurrentIndex(0);
}
