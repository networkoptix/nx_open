#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtCore/QEvent>
#include <QtCore/QSortFilterProxyModel>

#include <QtGui/QPainter>
#include <QtGui/QKeyEvent>

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QItemEditorFactory>
#include <QtWidgets/QComboBox>

#include <boost/algorithm/cxx11/any_of.hpp>

#include <api/app_server_connection.h>
#include <api/server_rest_connection.h>

// TODO: #vkutin Think of a better location and namespace.
#include <business/business_types_comparator.h>

#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <common/common_module.h>

#include <core/resource_access/resource_access_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/device_dependent_strings.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx_ec/dummy_handler.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/delegates/business_rule_item_delegate.h>
#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/common/item_view_auto_hider.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/watchers/workbench_safemode_watcher.h>

#include <utils/common/event_processors.h>
#include <utils/common/delayed.h>

using boost::algorithm::any_of;

using namespace nx;
using namespace nx::client::desktop::ui;

namespace {

    class SortRulesProxyModel:
        public QSortFilterProxyModel,
        public QnConnectionContextAware
    {
    public:
        explicit SortRulesProxyModel(QObject* parent = nullptr):
            QSortFilterProxyModel(parent),
            m_filterText(),
            m_lexComparator(new QnBusinessTypesComparator(true))
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
                    return lessThanByRole<vms::event::EventType>(left, right, Qn::EventTypeRole,
                        [this](vms::event::EventType left, vms::event::EventType right)
                        {
                            return m_lexComparator->lexicographicalLessThan(left, right);
                        });

                case Column::action:
                    return lessThanByRole<vms::event::ActionType>(left, right, Qn::ActionTypeRole,
                        [this](vms::event::ActionType left, vms::event::ActionType right)
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
            NX_ASSERT(idx.isValid(), Q_FUNC_INFO, "index must be valid here");
            if (!idx.isValid())
                return false;

            auto resourcePassText =
                [this](const QnResourcePtr& resource)
                {
                    return resource->toSearchString().contains(m_filterText, Qt::CaseInsensitive);
                };

            auto passText =
                [this, resourcePassText](const QnUuid& id)
                {
                    auto resource = resourcePool()->getResourceById(id);
                    if (resource)
                        return resourcePassText(resource);
                    auto role = userRolesManager()->userRole(id);
                    return role.name.contains(m_filterText, Qt::CaseInsensitive);
                };


            bool anyCameraPassFilter = any_of(resourcePool()->getAllCameras(QnResourcePtr(), true), resourcePassText);
            vms::event::EventType eventType = idx.data(Qn::EventTypeRole).value<vms::event::EventType>();
            if (vms::event::requiresCameraResource(eventType)) {
                auto eventResources = idx.data(Qn::EventResourcesRole).value<QSet<QnUuid>>();

                // rule supports any camera (assuming there is any camera that passing filter)
                if (eventResources.isEmpty() && anyCameraPassFilter)
                    return true;

                // rule contains camera passing the filter
                if (any_of(eventResources, passText))
                    return true;
            }

            vms::event::ActionType actionType = idx.data(Qn::ActionTypeRole).value<vms::event::ActionType>();
            if (vms::event::requiresCameraResource(actionType))
            {
                auto actionResources = idx.data(Qn::ActionResourcesRole).value<QSet<QnUuid>>();
                if (any_of(actionResources, passText))
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

}   //anonymous namespace


QnBusinessRulesDialog::QnBusinessRulesDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::BusinessRulesDialog()),
    m_popupMenu(new QMenu(this))
{
    ui->setupUi(this);

    ui->testRuleButton->setVisible(qnRuntime->isDevMode());

    retranslateUi();

    setWindowFlags(windowFlags()
        | Qt::WindowMaximizeButtonHint
        | Qt::MaximizeUsingFullscreenGeometryHint);

    QnSnappedScrollBar *scrollBar = new QnSnappedScrollBar(this);
    ui->tableView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    m_resetDefaultsButton = new QPushButton(tr("Restore All Rules to Default"));
    m_resetDefaultsButton->setEnabled(false);
    ui->buttonBox->addButton(m_resetDefaultsButton, QDialogButtonBox::ResetRole);
    connect(m_resetDefaultsButton, &QPushButton::clicked, this, &QnBusinessRulesDialog::at_resetDefaultsButton_clicked);

    setHelpTopic(this, Qn::EventsActions_Help);
    setHelpTopic(ui->eventLogButton, Qn::EventLog_Help);

    m_currentDetailsWidget = ui->detailsWidget;

    ui->eventLogButton->setIcon(qnSkin->icon(lit("buttons/event_log.png")));

    createActions();

    m_rulesViewModel = new QnBusinessRulesActualModel(this);

    /* Force column width must be set before table initializing because header options are not updated. */
    static const QList<Column> forcedColumns {
        Column::event,
        Column::action,
        Column::aggregation };

    for (auto column: forcedColumns)
    {
        m_rulesViewModel->forceColumnMinWidth(column,
            QnBusinessRuleItemDelegate::optimalWidth(column, this->fontMetrics()));
    }

    const auto kSortColumn = Column::event;

    SortRulesProxyModel* sortModel = new SortRulesProxyModel(this);
    sortModel->setDynamicSortFilter(false);
    sortModel->setSourceModel(m_rulesViewModel);
    sortModel->sort(int(kSortColumn));
    connect(ui->filterLineEdit, &QnSearchLineEdit::textChanged, sortModel, &SortRulesProxyModel::setText);

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

    ui->tableView->setItemDelegate(new QnBusinessRuleItemDelegate(this));

    connect(m_rulesViewModel, &QAbstractItemModel::dataChanged, this, &QnBusinessRulesDialog::at_model_dataChanged);
    connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &QnBusinessRulesDialog::at_tableView_currentRowChanged);

    ui->tableView->horizontalHeader()->setSortIndicator(int(kSortColumn), Qt::AscendingOrder);

    installEventHandler(ui->tableView->viewport(), QEvent::Resize, this,
        &QnBusinessRulesDialog::at_tableViewport_resizeEvent, Qt::QueuedConnection);

    // TODO: #GDM #Business show description label if no rules are loaded

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
        &QnBusinessRulesDialog::saveAll);

    ui->addRuleButton->setIcon(qnSkin->icon("buttons/plus.png"));
    connect(ui->addRuleButton, &QPushButton::clicked, this,
        &QnBusinessRulesDialog::at_newRuleButton_clicked);

    ui->deleteRuleButton->setIcon(qnSkin->icon("buttons/minus.png"));
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

    connect(ui->filterLineEdit, &QnSearchLineEdit::textChanged, this,
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
    auto autoHider = QnItemViewAutoHider::create(ui->tableView, tr("No event rules"));
    connect(autoHider, &QnItemViewAutoHider::viewVisibilityChanged, this, updateSelection);

    auto safeModeWatcher = new QnWorkbenchSafeModeWatcher(this);
    safeModeWatcher->addWarningLabel(ui->buttonBox);
    safeModeWatcher->addControlledWidget(m_resetDefaultsButton, QnWorkbenchSafeModeWatcher::ControlMode::Disable);
    safeModeWatcher->addControlledWidget(ui->buttonBox->button(QDialogButtonBox::Ok), QnWorkbenchSafeModeWatcher::ControlMode::Disable);
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
    m_pendingDeleteRules.removeOne(id); // TODO: #GDM #Business ask user
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    const int kInvalidSortingColumn = -1;

    m_rulesViewModel->createRule();
    // TODO: #GDM correct way will be return index and proxy it via sort model,
    //but without dynamic sorting it just works this way
    ui->tableView->selectRow(ui->tableView->model()->rowCount() - 1);
    ui->tableView->horizontalHeader()->setSortIndicator(kInvalidSortingColumn, Qt::AscendingOrder);
}

void QnBusinessRulesDialog::at_testRuleButton_clicked()
{
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
    if (!accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
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

    commonModule()->ec2Connection()->getBusinessEventManager(Qn::kSystemAccess)->resetBusinessRules(
        ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone );
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

void QnBusinessRulesDialog::at_resources_deleted( int handle, ec2::ErrorCode errorCode )
{
    if (!m_deleting.contains(handle))
        return;

    if(errorCode != ec2::ErrorCode::ok)
    {
        QnMessageBox::critical(this, ec2::toString(errorCode));
        m_pendingDeleteRules.append(m_deleting[handle]);
        return;
    }

    m_deleting.remove(handle);
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
    executeDelayedParented(handleRowChanged, 0, this);
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

bool QnBusinessRulesDialog::saveAll()
{
    QModelIndexList modified = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ModifiedRole, true, -1, Qt::MatchExactly);
    QModelIndexList invalid = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ValidRole, false, -1, Qt::MatchExactly);
    QSet<QModelIndex> invalid_modified = invalid.toSet().intersect(modified.toSet());

    if (!invalid_modified.isEmpty())
    {
        const QDialogButtonBox::StandardButton btn =
            QnMessageBox::warning(this,
                tr("Some rules are not valid. Disable them?"), QString(),
                QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
                QDialogButtonBox::Yes);

        switch (btn)
        {
            case QDialogButtonBox::Yes:
                foreach (QModelIndex idx, invalid_modified)
                {
                    m_rulesViewModel->rule(idx)->setDisabled(true);
                }
                break;
            case QDialogButtonBox::No:
                break;
            default:
                return false;
        }
    }

    foreach (QModelIndex idx, modified) {
        m_rulesViewModel->saveRule(idx);
    }

    // TODO: #GDM #Business replace with QnAppServerReplyProcessor
    foreach (const QnUuid& id, m_pendingDeleteRules) {
        int handle = commonModule()->ec2Connection()->getBusinessEventManager(Qn::kSystemAccess)->deleteRule(
            id, this, &QnBusinessRulesDialog::at_resources_deleted );
        m_deleting[handle] = id;
    }
    m_pendingDeleteRules.clear();
    return true;
}

void QnBusinessRulesDialog::testRule(const QnBusinessRuleViewModelPtr& ruleModel)
{
    auto server = commonModule()->currentServer();
    NX_EXPECT(server);
    if (!server)
        return;

    auto connection = server->restConnection();
    NX_EXPECT(connection);
    if (!connection)
        return;

    auto makeCallback =
        [this](const QString& text)
        {
            return
                [this, text](bool success, rest::Handle handle, QnJsonRestResult result)
                {
                    const QString details = result.errorString + L'\n' + result.reply.toString();
                    if (success)
                        QnMessageBox::information(this, text, details);
                    else
                        QnMessageBox::warning(this, text, details);
                };
        };

    auto eventType = ruleModel->eventType();
    if (nx::vms::event::hasToggleState(eventType))
    {
        connection->testEventRule(ruleModel->id(), nx::vms::event::EventState::active,
            [this, id = ruleModel->id(), connection, makeCallback]
            (bool success, rest::Handle handle, QnJsonRestResult result)
            {
                makeCallback(lit("Event Started"))(success, handle, result);
                if (success)
                {
                    connection->testEventRule(id, nx::vms::event::EventState::inactive,
                        makeCallback(lit("Event Stopped")), QThread::currentThread());
                }
            }, QThread::currentThread());
    }
    else
    {
        connection->testEventRule(ruleModel->id(), nx::vms::event::EventState::undefined,
            makeCallback(lit("Event Occurred")), QThread::currentThread());
    }

}

void QnBusinessRulesDialog::deleteRule(const QnBusinessRuleViewModelPtr& ruleModel)
{
    if (!ruleModel->id().isNull())
        m_pendingDeleteRules.append(ruleModel->id());
    m_rulesViewModel->deleteRule(ruleModel);
    updateControlButtons();
}

void QnBusinessRulesDialog::updateControlButtons() {
    bool hasRights = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission)
        && !commonModule()->isReadOnly();

    bool hasChanges = hasRights && (
                !m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ModifiedRole, true, 1, Qt::MatchExactly).isEmpty()
             || !m_pendingDeleteRules.isEmpty()
                );
    bool canDelete = hasRights && m_currentDetailsWidget->model();

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasRights);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasChanges);
    m_resetDefaultsButton->setEnabled(hasRights);

    ui->testRuleButton->setEnabled(canDelete);
    ui->deleteRuleButton->setEnabled(canDelete);
    m_deleteAction->setEnabled(canDelete);

    m_currentDetailsWidget->setVisible(hasRights && m_currentDetailsWidget->model());
    ui->addRuleButton->setEnabled(hasRights);
    m_newAction->setEnabled(hasRights);
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

    bool hasRights = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission) && !commonModule()->isReadOnly();
    bool hasChanges = hasRights && (
        !m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ModifiedRole, true, 1, Qt::MatchExactly).isEmpty()
        || !m_pendingDeleteRules.isEmpty()
        ); // TODO: #GDM #Business calculate once and use anywhere
    if (!hasChanges)
        return true;

    const auto result = QnMessageBox::question(this,
        tr("Apply changes before exit?"), QString(),
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
