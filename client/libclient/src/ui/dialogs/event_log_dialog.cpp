#include "event_log_dialog.h"
#include "ui_event_log_dialog.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <business/events/abstract_business_event.h>
#include <business/business_strings_helper.h>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/actions.h>
#include <ui/common/item_view_hover_tracker.h>
#include <ui/utils/table_export_helper.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/models/event_log_model.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/common/item_view_auto_hider.h>
#include <ui/widgets/common/snapped_scrollbar.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/event_processors.h>
#include <utils/common/delayed.h>

namespace {
    const int ProlongedActionRole = Qt::UserRole + 2;

    const int kQueryTimeoutMs = 15000;

    /* Really here can be any number, that is not equal to zero (success code). */
    const int kTimeoutStatus = -1;

    QnVirtualCameraResourceList cameras(const QSet<QnUuid>& ids)
    {
        return qnResPool->getResources<QnVirtualCameraResource>(ids);
    }
}


QnEventLogDialog::QnEventLogDialog(QWidget *parent):
    base_type(parent),
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

    ui->gridEvents->hoverTracker()->setAutomaticMouseCursor(true);

    //ui->gridEvents->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // init events model
    {
        QStandardItem* rootItem = createEventTree(0, QnBusiness::AnyBusinessEvent);
        m_eventTypesModel->appendRow(rootItem);
        ui->eventComboBox->setModel(m_eventTypesModel);
    }

    // init actions model
    {
        QStandardItem *anyActionItem = new QStandardItem(tr("Any Action"));
        anyActionItem->setData(QnBusiness::UndefinedAction);
        anyActionItem->setData(false, ProlongedActionRole);
        m_actionTypesModel->appendRow(anyActionItem);


        for (QnBusiness::ActionType actionType: QnBusiness::allActions()) {
            QStandardItem *item = new QStandardItem(QnBusinessStringsHelper::actionName(actionType));
            item->setData(actionType);
            item->setData(QnBusiness::hasToggleState(actionType), ProlongedActionRole);

            QList<QStandardItem *> row;
            row << item;
            m_actionTypesModel->appendRow(row);
        }
        ui->actionComboBox->setModel(m_actionTypesModel);
    }

    retranslateUi();

    m_filterAction      = new QAction(tr("Filter Similar Rows"), this);
    m_filterAction->setShortcut(Qt::ControlModifier + Qt::Key_F);
    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);
    m_resetFilterAction = new QAction(tr("Clear Filter"), this);
    m_resetFilterAction->setShortcut(Qt::ControlModifier + Qt::Key_R); //TODO: #Elric shouldn't we use QKeySequence::Refresh instead (evaluates to F5 on win)? --gdm

    installEventHandler(ui->gridEvents->viewport(), QEvent::MouseButtonRelease,
        this, &QnEventLogDialog::at_mouseButtonRelease);

    ui->gridEvents->addAction(m_clipboardAction);
    ui->gridEvents->addAction(m_exportAction);
    ui->gridEvents->addAction(m_filterAction);
    ui->gridEvents->addAction(m_resetFilterAction);

    ui->clearFilterButton->setIcon(qnSkin->icon("buttons/clear.png"));
    connect(ui->clearFilterButton, &QPushButton::clicked, this,
        &QnEventLogDialog::reset);

    ui->refreshButton->setIcon(qnSkin->icon("buttons/refresh.png"));
    ui->eventRulesButton->setIcon(qnSkin->icon("buttons/event_rules.png"));
    ui->loadingProgressBar->hide();

    QnSnappedScrollBar *scrollBar = new QnSnappedScrollBar(this);
    ui->gridEvents->setVerticalScrollBar(scrollBar->proxyScrollBar());

    connect(m_filterAction,         &QAction::triggered,                this,   &QnEventLogDialog::at_filterAction_triggered);
    connect(m_resetFilterAction,    &QAction::triggered,                this,   &QnEventLogDialog::reset);
    connect(m_clipboardAction,      &QAction::triggered,                this,   &QnEventLogDialog::at_clipboardAction_triggered);
    connect(m_exportAction,         &QAction::triggered,                this,   &QnEventLogDialog::at_exportAction_triggered);
    connect(m_selectAllAction,      &QAction::triggered,                ui->gridEvents, &QTableView::selectAll);

    connect(ui->dateRangeWidget, &QnDateRangeWidget::rangeChanged, this,
        &QnEventLogDialog::updateData);

    connect(ui->eventComboBox,      QnComboboxCurrentIndexChanged,      this,   &QnEventLogDialog::updateData);
    connect(ui->actionComboBox,     QnComboboxCurrentIndexChanged,      this,   &QnEventLogDialog::updateData);
    connect(ui->refreshButton,      &QAbstractButton::clicked,          this,   &QnEventLogDialog::updateData);
    connect(ui->eventRulesButton,   &QAbstractButton::clicked,          context()->action(QnActions::BusinessEventsAction), &QAction::trigger);

    connect(ui->cameraButton,       &QAbstractButton::clicked,          this,   &QnEventLogDialog::at_cameraButton_clicked);
    connect(ui->gridEvents,         &QTableView::clicked,               this,   &QnEventLogDialog::at_eventsGrid_clicked);
    connect(ui->gridEvents,         &QTableView::customContextMenuRequested, this, &QnEventLogDialog::at_eventsGrid_customContextMenuRequested);
    connect(qnSettings->notifier(QnClientSettings::EXTRA_INFO_IN_TREE), &QnPropertyNotifier::valueChanged, ui->gridEvents, &QAbstractItemView::reset);

    QnItemViewAutoHider::create(ui->gridEvents, tr("No events"));

    reset();
}

QnEventLogDialog::~QnEventLogDialog() {
}

QStandardItem* QnEventLogDialog::createEventTree(QStandardItem* rootItem, QnBusiness::EventType value)
{
    QStandardItem* item = new QStandardItem(QnBusinessStringsHelper::eventName(value));
    item->setData(value);

    if (rootItem)
        rootItem->appendRow(item);

    foreach(QnBusiness::EventType value, QnBusiness::childEvents(value))
        createEventTree(item, value);
    return item;
}

bool QnEventLogDialog::isFilterExist() const
{
    if (!cameras(m_filterCameraList).isEmpty())
        return true;

    QModelIndex idx = ui->eventComboBox->currentIndex();
    if (idx.isValid()) {
        QnBusiness::EventType eventType = (QnBusiness::EventType) m_eventTypesModel->itemFromIndex(idx)->data().toInt();
        if (eventType != QnBusiness::UndefinedEvent && eventType != QnBusiness::AnyBusinessEvent)
            return true;
    }

    if (ui->actionComboBox->currentIndex() > 0)
        return true;

    return false;
}

void QnEventLogDialog::reset()
{
    disableUpdateData();
    setEventType(QnBusiness::AnyBusinessEvent);
    setCameraList(QSet<QnUuid>());
    setActionType(QnBusiness::UndefinedAction);
    ui->dateRangeWidget->reset();
    enableUpdateData();
}

void QnEventLogDialog::updateData()
{
    if (m_updateDisabled) {
        m_dirty = true;
        return;
    }
    m_updateDisabled = true;

    QnBusiness::EventType eventType = QnBusiness::UndefinedEvent;
    {
        QModelIndex idx = ui->eventComboBox->currentIndex();
        if (idx.isValid())
            eventType = (QnBusiness::EventType) m_eventTypesModel->itemFromIndex(idx)->data().toInt();

        bool serverIssue = QnBusiness::parentEvent(eventType) == QnBusiness::AnyServerEvent || eventType == QnBusiness::AnyServerEvent;
        ui->cameraButton->setEnabled(!serverIssue);
        if (serverIssue)
            setCameraList(QSet<QnUuid>());

        bool istantOnly = !QnBusiness::hasToggleState(eventType) && eventType != QnBusiness::UndefinedEvent;
        updateActionList(istantOnly);
    }

    QnBusiness::ActionType actionType = QnBusiness::UndefinedAction;
    {
        int idx = ui->actionComboBox->currentIndex();
        actionType = (QnBusiness::ActionType) m_actionTypesModel->index(idx, 0).data(Qt::UserRole+1).toInt();
    }

    query(ui->dateRangeWidget->startTimeMs(),
          ui->dateRangeWidget->endTimeMs(),
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

    m_updateDisabled = false;
    m_dirty = false;
}

void QnEventLogDialog::query(qint64 fromMsec, qint64 toMsec,
                             QnBusiness::EventType eventType,
                             QnBusiness::ActionType actionType)
{
    m_requests.clear();
    m_allEvents.clear();

    const auto onlineServers = qnResPool->getAllServers(Qn::Online);
    for(const QnMediaServerResourcePtr& mserver: onlineServers)
    {
        int handle = mserver->apiConnection()->getEventLogAsync(
            fromMsec, toMsec,
            cameras(m_filterCameraList),
            eventType,
            actionType,
            QnUuid(),
            this, SLOT(at_gotEvents(int, const QnBusinessActionDataListPtr&, int)));

        m_requests << handle;

        const auto timerCallback =
            [this, handle]
            {
                at_gotEvents(kTimeoutStatus, QnBusinessActionDataListPtr(), handle);
            };

        executeDelayedParented(timerCallback, kQueryTimeoutMs, this);
    }
}

void QnEventLogDialog::retranslateUi()
{
    ui->retranslateUi(this);
    auto cameraList = cameras(m_filterCameraList);

    if (cameraList.empty())
        ui->cameraButton->selectAny();
    else
        ui->cameraButton->selectDevices(cameraList);

    /// Updates action type combobox model
    for (int row = 0; row != m_actionTypesModel->rowCount(); ++row)
    {
        const auto item = m_actionTypesModel->item(row);
        const auto type = static_cast<QnBusiness::ActionType>(item->data().toInt());
        if (type == QnBusiness::UndefinedAction)
            continue;

        const QString actionName = QnBusinessStringsHelper::actionName(type);
        item->setText(actionName);
    }

    ui->eventRulesButton->setVisible(menu()->canTrigger(QnActions::BusinessEventsAction));
}

void QnEventLogDialog::at_gotEvents(int httpStatus, const QnBusinessActionDataListPtr& events, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);

    if (httpStatus == 0 && events && !events->empty())
        m_allEvents << events;

    if (m_requests.isEmpty())
        requestFinished();
}

void QnEventLogDialog::requestFinished()
{
    m_model->setEvents(m_allEvents);
    m_allEvents.clear();
    ui->gridEvents->setDisabled(false);
    setCursor(Qt::ArrowCursor);
    auto start = ui->dateRangeWidget->startDate();
    auto end = ui->dateRangeWidget->endDate();

    if (start != end)
        ui->statusLabel->setText(tr("Event log for period from %1 to %2 - %n event(s) found", "", m_model->rowCount())
        .arg(start.toString(Qt::DefaultLocaleLongDate))
        .arg(end.toString(Qt::DefaultLocaleLongDate)));
    else
        ui->statusLabel->setText(tr("Event log for %1 - %n event(s) found", "", m_model->rowCount())
        .arg(start.toString(Qt::DefaultLocaleLongDate)));
    ui->loadingProgressBar->hide();
    ui->gridEvents->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void QnEventLogDialog::at_eventsGrid_clicked(const QModelIndex& idx)
{
    if (m_lastMouseButton != Qt::LeftButton)
        return;

    QnResourceList resources = m_model->resourcesForPlayback(idx);
    if (!resources.isEmpty())
    {
        qint64 pos = m_model->eventTimestamp(idx.row())/1000;
        QnActionParameters params(resources);
        params.setArgument(Qn::ItemTimeRole, pos);

        context()->menu()->trigger(QnActions::OpenInNewLayoutAction, params);

        if (isMaximized())
            showNormal();
    }
}

void QnEventLogDialog::setEventType(QnBusiness::EventType value)
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

void QnEventLogDialog::setDateRange(qint64 startTimeMs, qint64 endTimeMs)
{
    ui->dateRangeWidget->setRange(startTimeMs, endTimeMs);
}

void QnEventLogDialog::setCameraList(const QSet<QnUuid>& ids)
{
    if (ids == m_filterCameraList)
        return;

    m_filterCameraList = ids;
    retranslateUi();
    updateData();
}

void QnEventLogDialog::setActionType(QnBusiness::ActionType value)
{
    for (int i = 0; i < m_actionTypesModel->rowCount(); ++i)
    {
        QModelIndex idx = m_actionTypesModel->index(i, 0);
        if (idx.data(Qt::UserRole + 1).toInt() == value)
            ui->actionComboBox->setCurrentIndex(i);
    }
}

void QnEventLogDialog::at_filterAction_triggered()
{
    QModelIndex idx = ui->gridEvents->currentIndex();

    QnBusiness::EventType eventType = m_model->eventType(idx.row());
    QnBusiness::EventType parentEventType = QnBusiness::parentEvent(eventType);
    if (parentEventType != QnBusiness::AnyBusinessEvent && parentEventType != QnBusiness::UndefinedEvent)
        eventType = parentEventType;

    QSet<QnUuid> camList;
    const auto cameraResource = m_model->eventResource(idx.row()).dynamicCast<QnVirtualCameraResource>();
    if (cameraResource)
        camList << cameraResource->getId();

    disableUpdateData();
    setEventType(eventType);
    setCameraList(camList);
    setActionType(QnBusiness::UndefinedAction);
    enableUpdateData();
}

void QnEventLogDialog::at_eventsGrid_customContextMenuRequested(const QPoint&)
{
    QScopedPointer<QMenu> menu;
    QModelIndex idx = ui->gridEvents->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = m_model->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        QnActionManager *manager = context()->menu();
        if (resource)
        {
            QnActionParameters parameters(resource);
            parameters.setArgument(Qn::NodeTypeRole, Qn::ResourceNode);

            menu.reset(manager->newMenu(Qn::TreeScope, nullptr, parameters));
            foreach(QAction* action, menu->actions())
                action->setShortcut(QKeySequence());
        }
    }
    if (menu)
        menu->addSeparator();
    else
        menu.reset(new QMenu());

    m_filterAction->setEnabled(idx.isValid());
    m_clipboardAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());
    m_exportAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());

    menu->addAction(m_filterAction);
    menu->addAction(m_resetFilterAction);

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());
}

void QnEventLogDialog::at_exportAction_triggered()
{
    QnTableExportHelper::exportToFile(ui->gridEvents, true, this, tr("Export selected events to file"));
}

void QnEventLogDialog::at_clipboardAction_triggered()
{
    QnTableExportHelper::copyToClipboard(ui->gridEvents);
}

void QnEventLogDialog::at_mouseButtonRelease(QObject* sender, QEvent* event)
{
    Q_UNUSED(sender)
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

void QnEventLogDialog::at_cameraButton_clicked()
{
    QnResourceSelectionDialog dialog(QnResourceSelectionDialog::Filter::cameras, this);
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
    // TODO: #Elric use showEvent instead.

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
