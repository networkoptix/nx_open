// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <algorithm>

#include <QtCore/QEvent>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QItemEditorFactory>
#include <QtWidgets/QStyledItemDelegate>

#include <api/server_rest_connection.h>
// TODO: #vkutin Think of a better location and namespace.
#include <business/business_types_comparator.h>
#include <client/client_message_processor.h>
#include <client/client_runtime_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/random.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/item_view_auto_hider.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/search_helper.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/event/events/poe_over_budget_event.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <ui/delegates/business_rule_item_delegate.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/widgets/business/business_rule_widget.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/synctime.h>

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

static const QMap<QIcon::Mode, nx::vms::client::core::SvgIconColorer::ThemeColorsRemapData>
    kButtonsIconSubstitutions = {{QnIcon::Normal, {.primary = "light1"}}};

    class SortRulesProxyModel:
        public QSortFilterProxyModel,
        public nx::vms::client::core::CommonModuleAware
    {
    public:
        explicit SortRulesProxyModel(QObject* parent = nullptr):
            QSortFilterProxyModel(parent),
            m_filterText(),
            m_lexComparator(new QnBusinessTypesComparator())
        {
        }

        void setText(const QString& text)
        {
            if (m_filterText == text.trimmed())
                return;

            m_filterText = text.trimmed();
            invalidateFilter();
        }

    protected:
        using Column = QnBusinessRuleViewModel::Column;

        virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
        {
            switch (Column(sortColumn()))
            {
                case Column::modified:
                    return lessThanByRole<bool>(left, right, Qn::ModifiedRole);

                case Column::disabled:
                    return lessThanByRole<bool>(left, right, Qn::DisabledRole);

                case Column::event:
                    return lessThanByRole<vms::api::EventType>(left, right, Qn::EventTypeRole,
                        [this](vms::api::EventType left, vms::api::EventType right)
                        {
                            return m_lexComparator->lexicographicalLessThan(left, right);
                        });

                case Column::action:
                    return lessThanByRole<vms::api::ActionType>(left, right, Qn::ActionTypeRole,
                        [this](vms::api::ActionType left, vms::api::ActionType right)
                        {
                            return m_lexComparator->lexicographicalLessThan(left, right);
                        });

                case Column::source:
                case Column::target:
                case Column::aggregation:
                    return lessThanByRole<QString>(left, right, Qt::DisplayRole);
                default:
                    break;
            }

            return defaultLessThan(left, right);
        }

        virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override
        {
            // all rules should be visible if filter is empty
            if (m_filterText.isEmpty())
                return true;

            QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
            NX_ASSERT(idx.isValid(), "index must be valid here");
            if (!idx.isValid())
                return false;

            auto resourcePassText =
                [this](const QnResourcePtr& resource)
                {
                    return resources::search_helper::matches(m_filterText, resource);
                };

            auto passText =
                [this, resourcePassText](const QnUuid& id)
                {
                    auto resource = resourcePool()->getResourceById(id);
                    if (resource)
                        return resourcePassText(resource);
                    auto group = userGroupManager()->find(id);
                    return group && group->name.contains(m_filterText, Qt::CaseInsensitive);
                };

            const auto& cameras = resourcePool()->getAllCameras(QnResourcePtr(), true);
            bool anyCameraPassFilter = std::any_of(cameras.begin(), cameras.end(), resourcePassText);
            vms::api::EventType eventType = idx.data(Qn::EventTypeRole).value<vms::api::EventType>();
            if (vms::event::requiresCameraResource(eventType)) {
                auto eventResources = idx.data(Qn::EventResourcesRole).value<QSet<QnUuid>>();

                // rule supports any camera (assuming there is any camera that passing filter)
                if (eventResources.isEmpty() && anyCameraPassFilter)
                    return true;

                // rule contains camera passing the filter
                if (std::any_of(eventResources.begin(), eventResources.end(), passText))
                    return true;
            }

            vms::api::ActionType actionType = idx.data(Qn::ActionTypeRole).value<vms::api::ActionType>();
            if (vms::event::requiresCameraResource(actionType))
            {
                auto actionResources = idx.data(Qn::ActionResourcesRole).value<QSet<QnUuid>>();
                if (std::any_of(actionResources.begin(), actionResources.end(), passText))
                    return true;
            }

            return false;
        }

    private:
        bool defaultLessThan(const QModelIndex &left, const QModelIndex &right) const {
            return left.data(Qt::DisplayRole).toString() < right.data(Qt::DisplayRole).toString();
        };

        template <typename T>
        bool lessThanByRole(const QModelIndex &left, const QModelIndex &right, int role, std::function<bool (T left, T right)> comp = std::less<T>()) const {
            T lValue = left.data(role).value<T>();
            T rValue = right.data(role).value<T>();
            if (lValue != rValue)
                return comp(lValue, rValue);
            return defaultLessThan(left, right);
        }

    private:
        QString m_filterText;
        QScopedPointer<QnBusinessTypesComparator> m_lexComparator;
    };

    nx::network::rest::Params makeTestRuleParams(
        const QnBusinessRuleViewModelPtr& rule,
        QnResourcePool* resourcePool)
    {
        nx::network::rest::Params params;
        params.insert("event_type", nx::reflect::toString(rule->eventType()));
        params.insert("timestamp", QString::number(qnSyncTime->currentMSecsSinceEpoch()));

        if (rule->eventType() == nx::vms::api::EventType::pluginDiagnosticEvent)
        {
            QnUuid engineId = rule->eventParams().eventResourceId;

            if (engineId.isNull())
            {
                engineId = nx::utils::random::choice(
                    resourcePool->getResources<nx::vms::common::AnalyticsEngineResource>())->getId();
            }

            params.insert("eventResourceId", engineId.toString());
        }
        else if (rule->eventResources().size() > 0)
        {
            auto randomResource = nx::utils::random::choice(rule->eventResources());
            params.insert("eventResourceId", randomResource.toString());
        }
        else if (nx::vms::event::isSourceCameraRequired(rule->eventType())
            || rule->eventType() >= nx::vms::api::EventType::userDefinedEvent)
        {
            auto randomCamera = nx::utils::random::choice(
                resourcePool->getAllCameras(QnResourcePtr(), true));
            params.insert("eventResourceId", randomCamera->getId().toString());
        }
        else if (nx::vms::event::isSourceServerRequired(rule->eventType()))
        {
            auto randomServer = nx::utils::random::choice(
                resourcePool->getAllServers(nx::vms::api::ResourceStatus::online));
            params.insert("eventResourceId", randomServer->getId().toString());
        }

        if (rule->eventType() == nx::vms::api::EventType::poeOverBudgetEvent)
        {
            nx::vms::event::PoeOverBudgetEvent::Parameters poeParams;
            // TODO: #spanasenko Use actual values from the server configuration.
            poeParams.lowerLimitWatts = 0;
            poeParams.currentConsumptionWatts = 5;
            poeParams.currentConsumptionWatts = 5.5;
            params.insert("inputPortId", QJson::serialized(poeParams));
        }
        else
        {
            params.insert("inputPortId", rule->eventParams().inputPortId);
        }

        params.insert("source", rule->eventParams().resourceName);
        params.insert("caption", rule->eventParams().caption);
        params.insert("description", rule->eventParams().description);

        if (rule->eventType() == nx::vms::api::EventType::pluginDiagnosticEvent)
        {
            const auto& cameras = rule->eventResources();
            auto camera = cameras.isEmpty()
                ? nx::utils::random::choice(resourcePool->getAllCameras())->getId()
                : nx::utils::random::choice(cameras);

            using Level = nx::vms::api::EventLevel;
            using Levels = nx::vms::api::EventLevels;

            const auto levelFlags = nx::reflect::fromString<Levels>(
                rule->eventParams().inputPortId.toStdString());

            QVector<Level> levels;
            for (const auto& flag: {Level::ErrorEventLevel, Level::WarningEventLevel, Level::InfoEventLevel})
            {
                if (levelFlags & flag)
                    levels << flag;
            }

            nx::vms::event::EventMetaData data;
            data.level = levels.isEmpty() ? Level::UndefinedEventLevel : nx::utils::random::choice(levels);
            data.cameraRefs.push_back(camera.toString());

            params.insert("metadata", QString::fromUtf8(QJson::serialized(data)));
        }
        else if (rule->eventType() == nx::vms::api::EventType::cameraIpConflictEvent)
        {
            const auto camera = nx::utils::random::choice(resourcePool->getAllCameras());
            params.replace("caption", camera->getHostAddress());
            params.replace("description", camera->getMAC().toString());

            auto metadata = rule->eventParams().metadata;
            metadata.cameraRefs.push_back(camera->getId().toString());
            params.insert("metadata", QString::fromUtf8(QJson::serialized(metadata)));
        }
        else
        {
            params.insert("metadata", QString::fromUtf8(
                QJson::serialized(rule->eventParams().metadata)));
        }

        if (rule->eventType() == nx::vms::api::EventType::analyticsSdkEvent)
        {
            params.insert("analyticsEngineId", rule->eventParams().analyticsEngineId.toString());
        }

        return params;
    }

}   //anonymous namespace

QnBusinessRulesDialog::QnBusinessRulesDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::BusinessRulesDialog()),
    m_popupMenu(new QMenu(this))
{
    ui->setupUi(this);

    ui->testRuleButton->setVisible(ini().developerMode);

    retranslateUi();

    setWindowFlags(windowFlags()
        | Qt::WindowMaximizeButtonHint
        | Qt::MaximizeUsingFullscreenGeometryHint);

    SnappedScrollBar *scrollBar = new SnappedScrollBar(this);
    ui->tableView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    m_resetDefaultsButton = new QPushButton(tr("Restore All Rules to Default"));
    m_resetDefaultsButton->setEnabled(false);
    ui->buttonBox->addButton(m_resetDefaultsButton, QDialogButtonBox::ResetRole);
    connect(m_resetDefaultsButton, &QPushButton::clicked, this, &QnBusinessRulesDialog::at_resetDefaultsButton_clicked);

    setHelpTopic(this, HelpTopic::Id::EventsActions);
    setHelpTopic(ui->eventLogButton, HelpTopic::Id::EventLog);

    m_currentDetailsWidget = ui->detailsWidget;

    ui->eventLogButton->setIcon(qnSkin->icon("buttons/event_log_20.svg", kButtonsIconSubstitutions));

    createActions();

    m_rulesViewModel = new QnBusinessRulesActualModel(this);

    /* Force column width must be set before table initializing because header options are not updated. */
    static const QList<Column> forcedColumns {
        Column::event,
        Column::action,
        Column::aggregation };

    auto columnDelegate = new QnBusinessRuleItemDelegate(this);
    for (auto column: forcedColumns)
    {
        m_rulesViewModel->forceColumnMinWidth(column,
            columnDelegate->optimalWidth(column, this->fontMetrics()));
    }

    const auto kSortColumn = Column::event;

    SortRulesProxyModel* sortModel = new SortRulesProxyModel(this);
    sortModel->setDynamicSortFilter(false);
    sortModel->setSourceModel(m_rulesViewModel);
    sortModel->sort(int(kSortColumn));
    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, sortModel, &SortRulesProxyModel::setText);

    enum { kUpdateFilterDelayMs = 200 };
    ui->filterLineEdit->setTextChangedSignalFilterMs(kUpdateFilterDelayMs);

    ui->tableView->setModel(sortModel);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(false);

    ui->tableView->resizeColumnsToContents();

    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(int(Column::source), QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setSectionResizeMode(int(Column::target), QHeaderView::Interactive);

    for (auto column: forcedColumns)
        ui->tableView->horizontalHeader()->setSectionResizeMode(int(column), QHeaderView::Fixed);

    ui->tableView->installEventFilter(this);

    ui->tableView->setMinimumHeight(style::Metrics::kHeaderSize
        + style::Metrics::kButtonHeight * style::Hints::kMinimumTableRows);

    ui->tableView->setItemDelegate(columnDelegate);

    connect(m_rulesViewModel, &QAbstractItemModel::dataChanged, this, &QnBusinessRulesDialog::at_model_dataChanged);
    connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &QnBusinessRulesDialog::at_tableView_currentRowChanged);

    ui->tableView->horizontalHeader()->setSortIndicator(int(kSortColumn), Qt::AscendingOrder);

    installEventHandler(ui->tableView->viewport(), QEvent::Resize, this,
        &QnBusinessRulesDialog::at_tableViewport_resizeEvent, Qt::QueuedConnection);

    // TODO: #sivanov Show description label if no rules are loaded.

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
        &QnBusinessRulesDialog::saveAll);

    ui->addRuleButton->setIcon(qnSkin->icon("buttons/add_20x20.svg", kButtonsIconSubstitutions));
    connect(ui->addRuleButton, &QPushButton::clicked, this,
        &QnBusinessRulesDialog::at_newRuleButton_clicked);

    ui->deleteRuleButton->setIcon(qnSkin->icon("buttons/minus_20x20.svg", kButtonsIconSubstitutions));
    connect(ui->deleteRuleButton, &QPushButton::clicked, this,
        &QnBusinessRulesDialog::at_deleteButton_clicked);

    connect(m_rulesViewModel, &QnBusinessRulesActualModel::eventRuleDeleted, this,
        &QnBusinessRulesDialog::at_message_ruleDeleted);
    connect(m_rulesViewModel, &QnBusinessRulesActualModel::beforeModelChanged, this,
        &QnBusinessRulesDialog::at_beforeModelChanged);
    connect(m_rulesViewModel, &QnBusinessRulesActualModel::afterModelChanged, this,
        &QnBusinessRulesDialog::at_afterModelChanged);

    connect(ui->testRuleButton, &QPushButton::clicked, this,
        &QnBusinessRulesDialog::at_testRuleButton_clicked);

    connect(ui->eventLogButton, &QPushButton::clicked, this,
        [this]
        {
            context()->menu()->trigger(action::OpenBusinessLogAction,
                action::Parameters().withArgument(Qn::ParentWidgetRole, QPointer<QWidget>(this)));
        });

    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this,
        &QnBusinessRulesDialog::updateFilter);

    updateFilter();
    updateControlButtons();

    auto updateSelection =
        [this](bool tableIsEmpty)
        {
            if (!tableIsEmpty && !ui->tableView->currentIndex().isValid())
                ui->tableView->setCurrentIndex(ui->tableView->model()->index(0, 0));
        };

    updateSelection(ui->tableView->model()->rowCount() == 0);

    /*
    * Create auto-hider which will hide empty table and show a message instead. Table will be
    * reparented. Snapped scrollbar is already created and will stay in the correct parent.
    */
    auto autoHider = ItemViewAutoHider::create(ui->tableView, tr("No event rules"));
    connect(autoHider, &ItemViewAutoHider::viewVisibilityChanged, this, updateSelection);
}

QnBusinessRulesDialog::~QnBusinessRulesDialog() {
}

void QnBusinessRulesDialog::setFilter(const QString &filter) {
    ui->filterLineEdit->lineEdit()->setText(filter);
}

void QnBusinessRulesDialog::accept()
{
    if (!saveAll())
        return;

    base_type::accept();
}

void QnBusinessRulesDialog::reject() {
    if (!tryClose(false))
        return;

    base_type::reject();
}

bool QnBusinessRulesDialog::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        switch (pKeyEvent->key())
        {
            case Qt::Key_Delete:
#if defined(Q_OS_MAC)
            case Qt::Key_Backspace:
#endif
                at_deleteButton_clicked();
                return true;
            default:
                break;;
        }
    }
    else if (event->type() == QEvent::ContextMenu)
    {
        QContextMenuEvent *pContextEvent = static_cast<QContextMenuEvent*>(event);
        m_popupMenu->exec(pContextEvent->globalPos());
        return true;
    }
    return base_type::eventFilter(object, event);
}

void QnBusinessRulesDialog::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        event->ignore();
        return;
    default:
        break;
    }
    base_type::keyPressEvent(event);
}

void QnBusinessRulesDialog::showEvent( QShowEvent *event )
{
    base_type::showEvent(event);
    if (SortRulesProxyModel* sortModel = dynamic_cast<SortRulesProxyModel*>(ui->tableView->model()))
        sortModel->sort(ui->tableView->horizontalHeader()->sortIndicatorSection(), ui->tableView->horizontalHeader()->sortIndicatorOrder());

    ui->tableView->setCurrentIndex(QModelIndex());
    ui->filterLineEdit->setFocus();
}

void QnBusinessRulesDialog::at_beforeModelChanged() {
    m_currentDetailsWidget->setModel(QnBusinessRuleViewModelPtr());
    m_pendingDeleteRules.clear();
    m_deleting.clear();
    updateControlButtons();
}

void QnBusinessRulesDialog::at_message_ruleDeleted(const QnUuid &id) {
    m_pendingDeleteRules.removeOne(id); //< TODO: #sivanov Ask user.
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    const int kInvalidSortingColumn = -1;

    m_rulesViewModel->createRule();
    // TODO: #sivanov Correct way will be return index and proxy it via sort model,
    // but without dynamic sorting it just works this way.
    ui->tableView->selectRow(ui->tableView->model()->rowCount() - 1);
    ui->tableView->horizontalHeader()->setSortIndicator(kInvalidSortingColumn, Qt::AscendingOrder);
}

void QnBusinessRulesDialog::at_testRuleButton_clicked()
{
    if (hasChanges())
    {
        QnMessageBox::warning(
            this,
            "Apply changes",
            "[Dev] You need to save rules before testing any of them",
            QDialogButtonBox::Ok,
            QDialogButtonBox::Ok);
        return;
    }

    if (auto model = m_currentDetailsWidget->model())
        testRule(model);
}

void QnBusinessRulesDialog::at_deleteButton_clicked()
{
    if (auto model = m_currentDetailsWidget->model())
        deleteRule(model);
}

void QnBusinessRulesDialog::at_resetDefaultsButton_clicked()
{
    if (!systemContext()->accessController()->hasPowerUserPermissions())
        return;

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Restore all rules to default?"),
        tr("This action cannot be undone."),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        this);

    dialog.addCustomButton(QnMessageBoxCustomButton::Reset,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    if (auto connection = systemContext()->messageBusConnection())
    {
        connection->getEventRulesManager(Qn::kSystemAccess)->resetBusinessRules(
            [](int /*requestId*/, ec2::ErrorCode) {});
    }
}

void QnBusinessRulesDialog::at_afterModelChanged(QnBusinessRulesActualModelChange change, bool ok)
{
    if (!ok)
    {
        switch (change)
        {
            case RulesLoaded:
                QnMessageBox::critical(this, tr("Failed to retrieve rules"));
                break;
            case RuleSaved:
                QnMessageBox::critical(this, tr("Failed to save rule"));
                break;
        }
        return;
    }

    if (change == RulesLoaded)
    {
        updateFilter();
    }
    updateControlButtons();
}

void QnBusinessRulesDialog::at_tableView_currentRowChanged(const QModelIndex &current,
    const QModelIndex& /*previous*/)
{
    QnBusinessRuleViewModelPtr ruleModel = m_rulesViewModel->rule(current);
    QnUuid id = ruleModel ? ruleModel->id() : QnUuid();

    const auto handleRowChanged =
        [this, id]()
        {
            auto rule = m_rulesViewModel->ruleModelById(id);
            m_currentDetailsWidget->setModel(rule);
            updateControlButtons();
        };

    /**
     * Fixes QT bug when we able to select multiple rows even if we set single selection mode.
     * See VMS-5799.
     */
    executeLater(handleRowChanged, this);
}

void QnBusinessRulesDialog::at_tableViewport_resizeEvent() {
    QModelIndexList selectedIndices = ui->tableView->selectionModel()->selectedRows();
    if(selectedIndices.isEmpty())
        return;

    ui->tableView->scrollTo(selectedIndices.front());
}

void QnBusinessRulesDialog::at_model_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(bottomRight)
    if (topLeft.column() <= int(Column::modified) && bottomRight.column() >= int(Column::modified))
        updateControlButtons();

    updateFilter();
}

void QnBusinessRulesDialog::createActions() {
    m_newAction = new QAction(tr("&New..."), this);
    connect(m_newAction, &QAction::triggered, this, &QnBusinessRulesDialog::at_newRuleButton_clicked);

    m_deleteAction = new QAction(tr("&Delete"), this);
    connect(m_deleteAction, &QAction::triggered, this, &QnBusinessRulesDialog::at_deleteButton_clicked);

    QAction* scheduleAct = new QAction(tr("&Schedule..."), this);
    connect(scheduleAct, &QAction::triggered, m_currentDetailsWidget, &QnBusinessRuleWidget::at_scheduleButton_clicked);

    m_popupMenu->addAction(m_newAction);
    m_popupMenu->addAction(m_deleteAction);
    m_popupMenu->addSeparator();
    m_popupMenu->addAction(scheduleAct);
}

bool QnBusinessRulesDialog::hasEditPermissions() const
{
    return systemContext()->accessController()->hasPowerUserPermissions();
}

bool QnBusinessRulesDialog::hasChanges() const
{
    return hasEditPermissions()
        && (!m_pendingDeleteRules.isEmpty()
            || !m_rulesViewModel->match(
                m_rulesViewModel->index(0, 0), Qn::ModifiedRole, true, 1, Qt::MatchExactly).isEmpty());
}

bool QnBusinessRulesDialog::saveAll()
{
    auto connection = systemContext()->messageBusConnection();
    if (!connection)
        return false;

    QModelIndexList modified = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ModifiedRole, true, -1, Qt::MatchExactly);
    QModelIndexList invalid = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ValidRole, false, -1, Qt::MatchExactly);
    QSet<QModelIndex> invalid_modified = nx::utils::toQSet(invalid).intersect(nx::utils::toQSet(modified));

    if (!invalid_modified.isEmpty())
    {
        QnMessageBox::warning(this,
            tr("Some rules are not valid and may not work"),
            QString(),
            QDialogButtonBox::Ok,
            QDialogButtonBox::Ok);
    }

    foreach (QModelIndex idx, modified) {
        m_rulesViewModel->saveRule(idx);
    }

    foreach (const QnUuid& id, m_pendingDeleteRules)
    {
        int handle = connection->getEventRulesManager(Qn::kSystemAccess)->deleteRule(
            id,
            [this](int requestId, ec2::ErrorCode errorCode)
            {
                if (!m_deleting.contains(requestId))
                    return;

                if (errorCode != ec2::ErrorCode::ok)
                {
                    QnMessageBox::critical(this, ec2::toString(errorCode));
                    m_pendingDeleteRules.append(m_deleting[requestId]);
                    return;
                }

                m_deleting.remove(requestId);
                updateControlButtons();
            },
            this);
        m_deleting[handle] = id;
    }
    m_pendingDeleteRules.clear();
    return true;
}

void QnBusinessRulesDialog::testRule(const QnBusinessRuleViewModelPtr& ruleModel)
{
    using Callback = rest::ServerConnection::GetCallback;

    auto sendRequest =
        [this](
            nx::network::rest::Params params,
            nx::vms::api::EventState toggleState,
            Callback callback)
        {
            if (!connection())
                return;

            if (toggleState != nx::vms::api::EventState::undefined)
                params.insert("state", nx::reflect::toString(toggleState));

            connectedServerApi()->postJsonResult(
                "/api/createEvent",
                params,
                /*body*/ {},
                std::move(callback),
                this->thread());
        };

    auto makeCallback =
        [this](const QString& text)
        {
            return
                [this, text](
                    bool success, rest::Handle /*handle*/, nx::network::rest::JsonResult result)
                {
                    const QString details = result.errorString + '\n' + result.reply.toString();
                    if (success)
                        QnMessageBox::information(this, text, details);
                    else
                        QnMessageBox::warning(this, text, details);
                };
        };

    auto eventType = ruleModel->eventType();
    const auto params = makeTestRuleParams(ruleModel, resourcePool());

    if (nx::vms::event::hasToggleState(eventType, ruleModel->eventParams(), systemContext()))
    {
        sendRequest(params, nx::vms::api::EventState::active,
            [params, makeCallback, sendRequest](
                bool success, rest::Handle handle, nx::network::rest::JsonResult result)
            {
                makeCallback(lit("Event Started"))(success, handle, result);
                if (success)
                {
                    sendRequest(params, nx::vms::api::EventState::inactive,
                        makeCallback("Event Stopped"));
                }
            });
    }
    else
    {
        sendRequest(params, nx::vms::api::EventState::undefined, makeCallback("Event Occurred"));
    }

}

void QnBusinessRulesDialog::deleteRule(const QnBusinessRuleViewModelPtr& ruleModel)
{
    if (!ruleModel->id().isNull())
        m_pendingDeleteRules.append(ruleModel->id());
    m_rulesViewModel->deleteRule(ruleModel);
    updateControlButtons();
}

void QnBusinessRulesDialog::updateControlButtons()
{
    const bool hasPermissions = hasEditPermissions();
    const bool canModifyRule = hasPermissions && m_currentDetailsWidget->model();

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasPermissions);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasChanges());
    m_resetDefaultsButton->setEnabled(hasPermissions);

    ui->testRuleButton->setEnabled(canModifyRule);
    ui->deleteRuleButton->setEnabled(canModifyRule);
    m_deleteAction->setEnabled(canModifyRule);
    m_currentDetailsWidget->setVisible(canModifyRule);

    ui->addRuleButton->setEnabled(hasPermissions);
    m_newAction->setEnabled(hasPermissions);
}

void QnBusinessRulesDialog::updateFilter() {
    QString filter = ui->filterLineEdit->text();
    /* Don't allow empty filters. */
    if (!filter.isEmpty() && filter.trimmed().isEmpty()) {
        ui->filterLineEdit->clear(); /* Will call into this slot again, so it is safe to return. */
        return;
    }
}

void QnBusinessRulesDialog::retranslateUi()
{
    ui->retranslateUi(this);

    ui->filterLineEdit->lineEdit()->setPlaceholderText(
        QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Filter by devices..."),
        tr("Filter by cameras...")
    ));
}

bool QnBusinessRulesDialog::tryClose(bool force) {
    if (force || isHidden())
    {
        m_rulesViewModel->reset();
        hide();
        return true;
    }

    if (!hasChanges())
        return true;

    const auto result = QnMessageBox::question(this,
        tr("Apply changes before exiting?"),
        QString(),
        QDialogButtonBox::Apply | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
        QDialogButtonBox::Apply);

    switch (result)
    {
    case QDialogButtonBox::Apply:
        if (!saveAll())
            return false;   // Cancel was pressed in the confirmation dialog
        break;
    case QDialogButtonBox::Discard:
        m_rulesViewModel->reset();
        break;
    default:
        return false;   // Cancel was pressed
    }

    return true;
}
