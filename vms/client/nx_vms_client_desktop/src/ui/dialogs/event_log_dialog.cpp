#include "event_log_dialog.h"
#include "ui_event_log_dialog.h"

#include <chrono>

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <api/helpers/event_log_request_data.h>
#include <api/server_rest_connection.h>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <client/client_runtime_settings.h>

#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <nx/vms/client/desktop/analytics/analytics_entities_tree.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/resource_views/data/node_type.h>
#include <nx/vms/client/desktop/event_rules/nvr_events_actions_access.h>

#include <ui/utils/table_export_helper.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <ui/models/event_log_model.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <nx/vms/client/desktop/common/widgets/item_view_auto_hider.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/event_processors.h>
#include <utils/common/delayed.h>

#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/std/algorithm.h>

using namespace nx;
using namespace nx::vms::event;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

using namespace std::chrono;

namespace {

constexpr int kUpdateDelayMs = 1000;

enum EventListRoles
{
    EventTypeRole = Qt::UserRole + 1,
    AnalyticsEventTypeIdRole,
};

enum ActionListRoles
{
    ActionTypeRole = Qt::UserRole + 1,
    ProlongedActionRole,
};

ActionType actionType(const QModelIndex& index)
{
    return index.isValid()
        ? (ActionType) index.data(ActionTypeRole).toInt()
        : ActionType::undefinedAction;
}

EventType eventType(const QModelIndex& index)
{
    return index.isValid()
        ? (EventType) index.data(EventTypeRole).toInt()
        : EventType::undefinedEvent;
}

nx::analytics::EventTypeId analyticsEventTypeId(const QModelIndex& index)
{
    return index.isValid()
        ? index.data(AnalyticsEventTypeIdRole).value<nx::analytics::EventTypeId>()
        : nx::analytics::EventTypeId();
}

const int kQueryTimeoutMs = 15000;

QnVirtualCameraResourceList cameras(const QSet<QnUuid>& ids)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    return resourcePool->getResourcesByIds<QnVirtualCameraResource>(ids);
}

} // namespace

QnEventLogDialog::QnEventLogDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::EventLogDialog),
    m_eventTypesModel(new QStandardItemModel()),
    m_actionTypesModel(new QStandardItemModel()),
    m_updateDisabled(false),
    m_dirty(false),
    m_lastMouseButton(Qt::NoButton),
    m_helper(new StringsHelper(commonModule()))
{
    ui->setupUi(this);

    setWindowFlags(windowFlags()
        | Qt::WindowMaximizeButtonHint
        | Qt::MaximizeUsingFullscreenGeometryHint);

    setWarningStyle(ui->warningLabel);

    setHelpTopic(this, Qn::MainWindow_Notifications_EventLog_Help);

    QList<QnEventLogModel::Column> columns;
    columns << QnEventLogModel::DateTimeColumn << QnEventLogModel::EventColumn << QnEventLogModel::EventCameraColumn <<
        QnEventLogModel::ActionColumn << QnEventLogModel::ActionCameraColumn << QnEventLogModel::DescriptionColumn;

    m_model = new QnEventLogModel(this);
    m_model->setColumns(columns);
    ui->gridEvents->setModel(m_model);

    ui->gridEvents->hoverTracker()->setMouseCursorRole(Qn::ItemMouseCursorRole);

    //ui->gridEvents->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    initEventsModel();
    initActionsModel();

    retranslateUi();

    m_filterAction      = new QAction(tr("Filter Similar Rows"), this);
    m_filterAction->setShortcut(Qt::ControlModifier + Qt::Key_F);
    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);
    m_resetFilterAction = new QAction(tr("Clear Filter"), this);
    m_resetFilterAction->setShortcut(Qt::ControlModifier + Qt::Key_R); // TODO: #Elric shouldn't we use QKeySequence::Refresh instead (evaluates to F5 on win)? --gdm

    installEventHandler(ui->gridEvents->viewport(), QEvent::MouseButtonRelease,
        this, &QnEventLogDialog::at_mouseButtonRelease);

    ui->gridEvents->addAction(m_clipboardAction);
    ui->gridEvents->addAction(m_exportAction);
    ui->gridEvents->addAction(m_filterAction);
    ui->gridEvents->addAction(m_resetFilterAction);

    ui->clearFilterButton->setIcon(qnSkin->icon("text_buttons/clear.png"));
    connect(ui->clearFilterButton, &QPushButton::clicked, this,
        &QnEventLogDialog::reset);

    ui->refreshButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    ui->eventRulesButton->setIcon(qnSkin->icon("buttons/event_rules.png"));

    SnappedScrollBar *scrollBar = new SnappedScrollBar(this);
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
    connect(ui->eventRulesButton,   &QAbstractButton::clicked,          context()->action(action::BusinessEventsAction), &QAction::trigger);

    connect(ui->cameraButton,       &QAbstractButton::clicked,          this,   &QnEventLogDialog::at_cameraButton_clicked);
    connect(ui->gridEvents,         &QTableView::clicked,               this,   &QnEventLogDialog::at_eventsGrid_clicked);
    connect(ui->gridEvents,         &QTableView::customContextMenuRequested, this, &QnEventLogDialog::at_eventsGrid_customContextMenuRequested);
    connect(qnSettings->notifier(QnClientSettings::EXTRA_INFO_IN_TREE), &QnPropertyNotifier::valueChanged, ui->gridEvents, &QAbstractItemView::reset);

    connect(ui->textFilter, &QLineEdit::textChanged,
        this, &QnEventLogDialog::updateDataDelayed);
    m_delayUpdateTimer.setSingleShot(true);
    m_delayUpdateTimer.setInterval(kUpdateDelayMs);
    connect(&m_delayUpdateTimer, &QTimer::timeout, this, &QnEventLogDialog::updateData);

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::server) && !resource->hasFlags(Qn::fake))
                updateServerEventsMenu();
        });
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::server) && !resource->hasFlags(Qn::fake))
                updateServerEventsMenu();
        });

    ItemViewAutoHider::create(ui->gridEvents, tr("No events"));

    reset();
}

QnEventLogDialog::~QnEventLogDialog() {
}

QStandardItem* QnEventLogDialog::createEventTree(QStandardItem* rootItem, EventType value)
{
    auto item = new QStandardItem(m_helper->eventName(value));
    item->setData(value, EventTypeRole);

    if (rootItem)
        rootItem->appendRow(item);

    const auto accessibleEvents =
        NvrEventsActionsAccess::removeInacessibleNvrEvents(childEvents(value), resourcePool());

    for (auto childValue: accessibleEvents)
        createEventTree(item, childValue);

    if (value == EventType::analyticsSdkEvent)
        createAnalyticsEventTree(item);

    return item;
}

void QnEventLogDialog::createAnalyticsEventTree(QStandardItem* rootItem)
{
    auto addItem =
        [this](QStandardItem* parent, AnalyticsEntitiesTreeBuilder::NodePtr node)
        {
            auto item = new QStandardItem(node->text);
            item->setData(EventType::analyticsSdkEvent, EventTypeRole);
            item->setData(qVariantFromValue(node->entityId), AnalyticsEventTypeIdRole);
            item->setSelectable(node->nodeType == AnalyticsEntitiesTreeBuilder::NodeType::eventType);

            if (NX_ASSERT(parent))
                parent->appendRow(item);
            return item;
        };

    auto addItemRecursive = nx::utils::y_combinator(
        [addItem](auto addItemRecursive, auto parent, auto root) -> void
        {
            for (auto node: root->children)
            {
                const auto menuItem = addItem(parent, node);
                addItemRecursive(menuItem, node);
            }
            if (!root->children.empty())
                parent->sortChildren(0);
        });

    auto analyticsEventsSearchTreeBuilder =
        commonModule()->instance<AnalyticsEventsSearchTreeBuilder>();
    const auto root = analyticsEventsSearchTreeBuilder->eventTypesTree();
    addItemRecursive(rootItem, root);
}

void QnEventLogDialog::updateAnalyticsEvents()
{
    const auto found = m_eventTypesModel->match(
        m_eventTypesModel->index(0, 0),
        EventTypeRole,
        /*value*/ EventType::analyticsSdkEvent,
        /*hits*/ 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    NX_ASSERT(found.size() ==  1);
    if (found.size() != 1)
        return;

    const auto index = found[0];
    auto item = m_eventTypesModel->itemFromIndex(index);
    NX_ASSERT(item);
    if (!item)
        return;

    const auto selectedIndex = ui->eventComboBox->currentIndex();
    const auto selectedEventType = selectedIndex.data(EventTypeRole).toInt();
    const auto selectedAnalyticsEventTypeId = selectedEventType == EventType::analyticsSdkEvent
        ? analyticsEventTypeId(selectedIndex)
        : nx::analytics::EventTypeId();

    item->removeRows(0, item->rowCount());
    createAnalyticsEventTree(item);

    if (!selectedAnalyticsEventTypeId.isNull())
    {
        const auto newlyCreatedItem = m_eventTypesModel->match(
            m_eventTypesModel->index(0, 0),
            AnalyticsEventTypeIdRole,
            /*value*/ qVariantFromValue(selectedAnalyticsEventTypeId),
            /*hits*/ 1,
            Qt::MatchExactly | Qt::MatchRecursive);

        if (newlyCreatedItem.size() == 1)
            ui->eventComboBox->setCurrentIndex(newlyCreatedItem[0]);
        else
            ui->eventComboBox->setCurrentIndex(index);
    }
}

void QnEventLogDialog::updateServerEventsMenu()
{
    std::function<QModelIndex(const QModelIndex&, EventType)> findIndexByEventType;
    findIndexByEventType =
        [this, &findIndexByEventType]
        (const QModelIndex& rootIndex, EventType eventType) -> QModelIndex
        {
            for (int row = 0; row < m_eventTypesModel->rowCount(rootIndex); ++row)
            {
                const auto rowIndex = m_eventTypesModel->index(row, 0, rootIndex);
                const auto eventTypeData = rowIndex.data(EventTypeRole);
                if (eventTypeData.isNull())
                    continue;

                if (eventTypeData.value<EventType>() == eventType)
                    return rowIndex;

                const auto childIndex = findIndexByEventType(rowIndex, eventType);
                if (childIndex != QModelIndex())
                    return childIndex;
            }
            return QModelIndex();
        };

    const auto anyServerEventIndex =
        findIndexByEventType(QModelIndex(), EventType::anyServerEvent);
    if (anyServerEventIndex == QModelIndex())
        return;

    const auto anyServerEventItem = m_eventTypesModel->itemFromIndex(anyServerEventIndex);
    const auto accessibleEvents = NvrEventsActionsAccess::removeInacessibleNvrEvents(
        childEvents(EventType::anyServerEvent), resourcePool());

    auto selectedEventType = eventType(ui->eventComboBox->currentIndex());

    anyServerEventItem->removeRows(0, anyServerEventItem->rowCount());
    for (auto childValue: accessibleEvents)
        createEventTree(anyServerEventItem, childValue);

    if (parentEvent(selectedEventType) == EventType::anyServerEvent)
    {
        if (!accessibleEvents.contains(selectedEventType))
            selectedEventType = EventType::anyServerEvent;
        ui->eventComboBox->setCurrentIndex(findIndexByEventType(QModelIndex(), selectedEventType));
    }
}

bool QnEventLogDialog::isFilterExist() const
{
    if (!cameras(m_filterCameraList).isEmpty())
        return true;

    const auto eventType = ::eventType(ui->eventComboBox->currentIndex());
    if (eventType != EventType::undefinedEvent && eventType != EventType::anyEvent)
        return true;

    if (ui->actionComboBox->currentIndex() > 0)
        return true;

    return false;
}

void QnEventLogDialog::initEventsModel()
{
    QStandardItem* rootItem = createEventTree(nullptr, EventType::anyEvent);
    m_eventTypesModel->appendRow(rootItem);
    ui->eventComboBox->setModel(m_eventTypesModel);

    auto updateAnalyticsSubmenuOperation = new nx::utils::PendingOperation(
        [this] { updateAnalyticsEvents(); },
        100ms,
        this);
    updateAnalyticsSubmenuOperation->fire();
    updateAnalyticsSubmenuOperation->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);

    connect(commonModule()->instance<AnalyticsEventsSearchTreeBuilder>(),
        &AnalyticsEventsSearchTreeBuilder::eventTypesTreeChanged,
        updateAnalyticsSubmenuOperation,
        &nx::utils::PendingOperation::requestOperation);
}

void QnEventLogDialog::initActionsModel()
{
    QStandardItem* anyActionItem = new QStandardItem(tr("Any Action"));
    anyActionItem->setData(ActionType::undefinedAction, ActionTypeRole);
    anyActionItem->setData(false, ProlongedActionRole);
    m_actionTypesModel->appendRow(anyActionItem);

    const auto accessibleActions =
        NvrEventsActionsAccess::removeInacessibleNvrActions(allActions(), resourcePool());

    for (ActionType actionType: accessibleActions)
    {
        QStandardItem* item = new QStandardItem(m_helper->actionName(actionType));
        item->setData(actionType, ActionTypeRole);
        item->setData(hasToggleState(actionType), ProlongedActionRole);

        m_actionTypesModel->appendRow(item);
    }
    ui->actionComboBox->setModel(m_actionTypesModel);
}

void QnEventLogDialog::reset()
{
    disableUpdateData();
    setEventType(EventType::anyEvent);
    setCameraList(QSet<QnUuid>());
    setActionType(ActionType::undefinedAction);
    setText(QString());
    ui->dateRangeWidget->reset();
    enableUpdateData();
}

void QnEventLogDialog::updateDataDelayed()
{
    m_delayUpdateTimer.stop();
    m_delayUpdateTimer.start();
}

void QnEventLogDialog::updateData()
{
    if (m_updateDisabled)
    {
        m_dirty = true;
        return;
    }
    m_updateDisabled = true;
    m_delayUpdateTimer.stop();
    EventType eventType = ::eventType(ui->eventComboBox->currentIndex());
    bool serverIssue = parentEvent(eventType) == EventType::anyServerEvent
        || eventType == EventType::anyServerEvent;
    ui->cameraButton->setEnabled(!serverIssue);
    if (serverIssue)
        setCameraList(QSet<QnUuid>());

    bool istantOnly = !hasToggleState(eventType, vms::event::EventParameters(), commonModule())
        && eventType != EventType::undefinedEvent;

    updateActionList(istantOnly);

    const auto actionType = ::actionType(m_actionTypesModel->index(
        ui->actionComboBox->currentIndex(), 0));

    const auto analyticsEventTypeId = eventType == EventType::analyticsSdkEvent
        ? ::analyticsEventTypeId(ui->eventComboBox->currentIndex())
        : nx::analytics::EventTypeId();

    query(ui->dateRangeWidget->startTimeMs(),
        ui->dateRangeWidget->endTimeMs(),
        eventType,
        analyticsEventTypeId,
        actionType,
        ui->textFilter->text());

    // update UI

    m_resetFilterAction->setEnabled(isFilterExist());
    if (!m_requests.isEmpty())
    {
        ui->stackedWidget->setCurrentWidget(ui->progressPage);
        setCursor(Qt::BusyCursor);
    }
    else
    {
        requestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    m_updateDisabled = false;
    m_dirty = false;
}

void QnEventLogDialog::query(qint64 fromMsec,
    qint64 toMsec,
    EventType eventType,
    const nx::analytics::EventTypeId& analyticsEventTypeId,
    ActionType actionType,
    const QString& text)
{
    m_requests.clear();
    m_allEvents.clear();

    QnEventLogRequestData request;
    request.filter.cameras = cameras(m_filterCameraList);
    request.filter.period = QnTimePeriod(fromMsec, toMsec - fromMsec);
    request.filter.eventType = eventType;
    request.filter.eventSubtype = analyticsEventTypeId;
    request.filter.actionType = actionType;
    request.filter.text = text;

    QPointer<QnEventLogDialog> guard(this);
    auto callback =
        [this, guard](bool success, rest::Handle handle, rest::EventLogData data)
        {
            if (!guard)
                return;

            if (!m_requests.contains(handle))
                return;
            m_requests.remove(handle);

            if (success)
            {
                auto& events = data.data;
                m_allEvents.reserve(m_allEvents.size() + events.size());
                std::move(events.begin(), events.end(), std::back_inserter(m_allEvents));
            }
            else if (!success)
            {
                NX_DEBUG(this, lm("Error %1 while requesting even log (%2)")
                    .args(data.error, data.errorString));
            }

            if (m_requests.isEmpty())
                requestFinished();
        };

    const auto onlineServers = resourcePool()->getAllServers(Qn::Online);
    for (const QnMediaServerResourcePtr& mserver: onlineServers)
    {
        auto handle = mserver->restConnection()->getEvents(request,
            callback, this->thread());

        if (handle <= 0)
            continue;

        m_requests << handle;

        const auto timerCallback =
            [handle, callback]
            {
                callback(false, handle, rest::EventLogData());
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
        const auto type = static_cast<ActionType>(item->data().toInt());
        if (type == ActionType::undefinedAction)
            continue;

        const QString actionName = m_helper->actionName(type);
        item->setText(actionName);
    }

    ui->eventRulesButton->setVisible(menu()->canTrigger(action::BusinessEventsAction));
}

void QnEventLogDialog::requestFinished()
{
    m_model->setEvents(std::move(m_allEvents));
    m_allEvents.clear();
    setCursor(Qt::ArrowCursor);

    auto start = ui->dateRangeWidget->startDate();
    auto end = ui->dateRangeWidget->endDate();
    if (start != end)
    {
        ui->statusLabel->setText(
            tr("Event log for period from %1 to %2 - %n events found",
                "Dates are substituted", m_model->rowCount())
            .arg(start.toString(Qt::DefaultLocaleLongDate))
            .arg(end.toString(Qt::DefaultLocaleLongDate)));
    }
    else
    {
        ui->statusLabel->setText(
            tr("Event log for %1 - %n events found", "Date is substituted", m_model->rowCount())
            .arg(start.toString(Qt::DefaultLocaleLongDate)));
    }

    ui->gridEvents->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    ui->stackedWidget->setCurrentWidget(ui->gridPage);
}

void QnEventLogDialog::at_eventsGrid_clicked(const QModelIndex& idx)
{
    if (m_lastMouseButton != Qt::LeftButton)
        return;

    QnResourceList resources = m_model->resourcesForPlayback(idx).filtered(
        [this](const QnResourcePtr& resource)
        {
            return accessController()->hasPermissions(resource, Qn::ReadPermission);
        });

    if (!resources.isEmpty())
    {
        action::Parameters params(resources);

        const auto timePos = m_model->eventTimestamp(idx.row());
        if (timePos != AV_NOPTS_VALUE)
        {
            qint64 pos = timePos / 1000;
            params.setArgument(Qn::ItemTimeRole, pos);
        }

        context()->menu()->trigger(action::OpenInNewTabAction, params);

        if (isMaximized())
            showNormal();
    }
}

void QnEventLogDialog::setEventType(EventType value)
{
    const auto found = m_eventTypesModel->match(
        m_eventTypesModel->index(0, 0),
        EventTypeRole,
        /*value*/value,
        /*hits*/ 1,
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

QnUuidSet QnEventLogDialog::getCameraList() const
{
    return m_filterCameraList;
}

void QnEventLogDialog::setCameraList(const QnUuidSet& ids)
{
    if (ids == m_filterCameraList)
        return;

    m_filterCameraList = ids;
    retranslateUi();
    updateAnalyticsEvents();
    updateData();
}

void QnEventLogDialog::setActionType(ActionType value)
{
    for (int i = 0; i < m_actionTypesModel->rowCount(); ++i)
    {
        if (actionType(m_actionTypesModel->index(i, 0)) == value)
        {
            ui->actionComboBox->setCurrentIndex(i);
            break;
        }
    }
}

void QnEventLogDialog::setText(const QString& text)
{
    ui->textFilter->setText(text);
}

void QnEventLogDialog::at_filterAction_triggered()
{
    QModelIndex idx = ui->gridEvents->currentIndex();

    EventType eventType = m_model->eventType(idx.row());
    EventType parentEventType = parentEvent(eventType);
    if (parentEventType != EventType::anyEvent && parentEventType != EventType::undefinedEvent)
        eventType = parentEventType;

    QSet<QnUuid> camList;
    const auto cameraResource =
        m_model->eventResource(idx.row()).dynamicCast<QnVirtualCameraResource>();
    if (cameraResource)
        camList << cameraResource->getId();

    disableUpdateData();
    setEventType(eventType);
    setCameraList(camList);
    setActionType(ActionType::undefinedAction);
    enableUpdateData();
}

void QnEventLogDialog::at_eventsGrid_customContextMenuRequested(const QPoint&)
{
    QScopedPointer<QMenu> menu;
    QModelIndex idx = ui->gridEvents->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = m_model->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        auto manager = context()->menu();
        if (resource && accessController()->hasPermissions(resource, Qn::ViewContentPermission))
        {
            action::Parameters parameters(resource);
            parameters.setArgument(Qn::NodeTypeRole, ResourceTreeNodeType::resource);

            menu.reset(manager->newMenu(action::TreeScope, nullptr, parameters));
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
    QnTableExportHelper::exportToFile(
        ui->gridEvents->model(),
        ui->gridEvents->selectionModel()->selectedIndexes(),
        this,
        tr("Export selected events to file"));
}

void QnEventLogDialog::at_clipboardAction_triggered()
{
    QnTableExportHelper::copyToClipboard(
        ui->gridEvents->model(),
        ui->gridEvents->selectionModel()->selectedIndexes());
}

void QnEventLogDialog::at_mouseButtonRelease(QObject* sender, QEvent* event)
{
    Q_UNUSED(sender)
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

void QnEventLogDialog::at_cameraButton_clicked()
{
    auto cameraIds = getCameraList();
    auto dialogAccepted = CameraSelectionDialog::selectCameras
        <CameraSelectionDialog::DummyPolicy>(cameraIds, this);
    if (dialogAccepted)
        setCameraList(cameraIds);
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

void QnEventLogDialog::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    m_dirty = true;
    enableUpdateData();
}

void QnEventLogDialog::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);
    disableUpdateData();
}

void QnEventLogDialog::updateActionList(bool instantOnly)
{
    const auto prolongedActions = m_actionTypesModel->match(
        m_actionTypesModel->index(0, 0),
        ProlongedActionRole,
        /*value*/ true,
        /*hits*/ -1,
        Qt::MatchExactly);

    // what type of actions to show: prolonged or instant
    bool enableProlongedActions = !instantOnly;
    for (const auto& idx: prolongedActions)
    {
        m_actionTypesModel->item(idx.row())->setEnabled(enableProlongedActions);
        m_actionTypesModel->item(idx.row())->setSelectable(enableProlongedActions);
    }

    if (!m_actionTypesModel->item(ui->actionComboBox->currentIndex())->isEnabled())
        ui->actionComboBox->setCurrentIndex(0);
}
