#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtCore/QEvent>

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QItemEditorFactory>
#include <QtWidgets/QComboBox>
#include <QtGui/QPainter>
#include <QtGui/QKeyEvent>

#include <api/app_server_connection.h>

#include <utils/common/event_processors.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>

#include <nx_ec/dummy_handler.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/delegates/business_rule_item_delegate.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>

QnBusinessRulesDialog::QnBusinessRulesDialog(QWidget *parent):
    base_type(parent, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowSystemMenuHint | Qt::WindowContextHelpButtonHint | Qt::WindowCloseButtonHint
#ifdef Q_OS_MAC
    | Qt::Tool
#endif
    ),
    QnWorkbenchContextAware(parent),
    ui(new Ui::BusinessRulesDialog()),
    m_popupMenu(new QMenu(this)),
    m_advancedAction(NULL),
    m_advancedMode(false)
{
    ui->setupUi(this);

    m_resetDefaultsButton = new QPushButton(tr("Reset Default Rules"));
    m_resetDefaultsButton->setEnabled(false);
    ui->buttonBox->addButton(m_resetDefaultsButton, QDialogButtonBox::ResetRole);
    connect(m_resetDefaultsButton, SIGNAL(clicked()), this, SLOT(at_resetDefaultsButton_clicked()));

    setHelpTopic(this, Qn::EventsActions_Help);

    m_currentDetailsWidget = ui->detailsWidget;

    createActions();

    m_rulesViewModel = new QnBusinessRulesActualModel(this);

    ui->tableView->setModel(m_rulesViewModel);
    ui->tableView->horizontalHeader()->setVisible(true);

    ui->tableView->resizeColumnsToContents();

    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnBusiness::EventColumn, QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnBusiness::SourceColumn, QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnBusiness::ActionColumn, QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnBusiness::TargetColumn, QHeaderView::Interactive);

    ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
    ui->tableView->installEventFilter(this);

    ui->tableView->setItemDelegate(new QnBusinessRuleItemDelegate(this));

    connect(m_rulesViewModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_model_dataChanged(QModelIndex,QModelIndex)));
    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));

    ui->tableView->clearSelection();

    // TODO: #Elric replace with a single connect call
    QnSingleEventSignalizer *resizeSignalizer = new QnSingleEventSignalizer(this);
    resizeSignalizer->setEventType(QEvent::Resize);
    ui->tableView->viewport()->installEventFilter(resizeSignalizer);
    connect(resizeSignalizer, SIGNAL(activated(QObject *, QEvent *)), this, SLOT(at_tableViewport_resizeEvent()), Qt::QueuedConnection);


    //TODO: #GDM show description label if no rules are loaded

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(at_saveAllButton_clicked()));
    connect(ui->addRuleButton,                              SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));
    connect(ui->deleteRuleButton,                           SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
    connect(ui->advancedButton,                             SIGNAL(clicked()), this, SLOT(toggleAdvancedMode()));

    connect(m_rulesViewModel,                               SIGNAL(businessRuleDeleted(QnId)),
            this, SLOT(at_message_ruleDeleted(QnId)));

    connect(ui->eventLogButton,   SIGNAL(clicked(bool)),              
            context()->action(Qn::BusinessEventsLogAction), SIGNAL(triggered()));

    connect(m_rulesViewModel,   SIGNAL(beforeModelChanged()), this, SLOT(at_beforeModelChanged()));

    connect(m_rulesViewModel,   SIGNAL(afterModelChanged(QnBusinessRulesActualModelChange, bool)),
            this,               SLOT(at_afterModelChanged(QnBusinessRulesActualModelChange, bool)));

    m_rulesViewModel->reloadData();

    connect(ui->filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(updateFilter()));
    connect(ui->clearFilterButton, SIGNAL(clicked()), this, SLOT(at_clearFilterButton_clicked()));
    updateFilter();
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

void QnBusinessRulesDialog::setFilter(const QString &filter) {
    ui->filterLineEdit->setText(filter);
}

void QnBusinessRulesDialog::accept()
{
    if (!saveAll())
        return;

    base_type::accept();
}

void QnBusinessRulesDialog::reject() {

    bool hasRights = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;
    bool loaded = m_rulesViewModel->isLoaded();
    bool hasChanges = hasRights && loaded && (
                !m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ModifiedRole, true, 1, Qt::MatchExactly).isEmpty()
             || !m_pendingDeleteRules.isEmpty()
                ); //TODO: #GDM calculate once and use anywhere
    if (!hasChanges) {
        base_type::reject();
        return;
    }

    QMessageBox::StandardButton btn =  QMessageBox::question(this,
                      tr("Confirm exit"),
                      tr("Unsaved changes will be lost. Save?"),
                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                      QMessageBox::Cancel);

    switch (btn) {
    case QMessageBox::Yes:
        if (!saveAll())
            return;
        break;
    case QMessageBox::No:
        m_rulesViewModel->reloadData();
        break;
    default:
        return;
    }
    base_type::reject();
}

bool QnBusinessRulesDialog::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        if (pKeyEvent->key() == Qt::Key_Delete) {
            at_deleteButton_clicked();
            return true;
        }
    } else if (event->type() == QEvent::ContextMenu) {
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

void QnBusinessRulesDialog::at_beforeModelChanged() {
   // bool enabled = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;
    m_currentDetailsWidget->setModel(NULL);
    m_pendingDeleteRules.clear();
    m_deleting.clear();
    updateControlButtons();
}

void QnBusinessRulesDialog::at_message_ruleDeleted(QnId id) {
    m_pendingDeleteRules.removeOne(id); //TODO: #GDM ask user
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    m_rulesViewModel->addRule(QnBusinessEventRulePtr());
    ui->tableView->setCurrentIndex(m_rulesViewModel->index(m_rulesViewModel->rowCount() - 1, 0));
    if (m_rulesViewModel->rowCount() == 1) {
        ui->tableView->resizeColumnsToContents();
        ui->tableView->horizontalHeader()->setStretchLastSection(true);
        ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
    }
}

void QnBusinessRulesDialog::at_saveAllButton_clicked() {
    saveAll();
}

void QnBusinessRulesDialog::at_deleteButton_clicked() {
    QnBusinessRuleViewModel* model = m_currentDetailsWidget->model();
    if (!model)
        return;
    if (model->system())
        return;
    deleteRule(model);
}

void QnBusinessRulesDialog::at_resetDefaultsButton_clicked() {
    if (!(accessController()->globalPermissions() & Qn::GlobalProtectedPermission))
        return;

    if (!m_rulesViewModel->isLoaded())
        return;

    if (QMessageBox::warning(this,
                             tr("Confirm rules reset"),
                             tr("Are you sure you want to reset rules to the defaults?\n"\
                                "This action CANNOT be undone!"),
                             QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
                             QMessageBox::Cancel) == QMessageBox::Cancel)
        return;

    QnAppServerConnectionFactory::getConnection2()->getBusinessEventManager()->resetBusinessRules(
        ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone );
//  m_rulesViewModel->clear();
//  updateControlButtons();
}

void QnBusinessRulesDialog::at_clearFilterButton_clicked() {
    ui->filterLineEdit->clear();
}

void QnBusinessRulesDialog::at_afterModelChanged(QnBusinessRulesActualModelChange change, bool ok) {
    if (!ok) {
        switch (change) {
        case RulesLoaded:
            QMessageBox::critical(this, tr("Error"), tr("Error while receiving rules."));
            break;
        case RuleSaved:
            QMessageBox::critical(this, tr("Error"), tr("Error while saving rule."));
            break;
        }
        return;
    }

    if (change == RulesLoaded) {
        ui->tableView->resizeColumnsToContents();
        ui->tableView->horizontalHeader()->setStretchLastSection(true);
        ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
        updateFilter();
    }
    updateControlButtons();
}

void QnBusinessRulesDialog::at_resources_deleted( int handle, ec2::ErrorCode errorCode ) {
    if (!m_deleting.contains(handle))
        return;

    if( errorCode != ec2::ErrorCode::ok ) {
        QMessageBox::critical(this, tr("Error while deleting rule"), ec2::toString(errorCode));
        m_pendingDeleteRules.append(m_deleting[handle]);
        return;
    }
    m_deleting.remove(handle);
    updateControlButtons();
}

void QnBusinessRulesDialog::at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous)

    QnBusinessRuleViewModel* ruleModel = m_rulesViewModel->getRuleModel(current.row());
    m_currentDetailsWidget->setModel(ruleModel);

    updateControlButtons();
}

void QnBusinessRulesDialog::at_tableViewport_resizeEvent() {
    QModelIndexList selectedIndices = ui->tableView->selectionModel()->selectedRows();
    if(selectedIndices.isEmpty())
        return;
    
    ui->tableView->scrollTo(selectedIndices.front());
}

void QnBusinessRulesDialog::at_model_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    Q_UNUSED(bottomRight)
    if (topLeft.column() <= QnBusiness::ModifiedColumn && bottomRight.column() >= QnBusiness::ModifiedColumn)
        updateControlButtons();
}

void QnBusinessRulesDialog::toggleAdvancedMode() {
    setAdvancedMode(!advancedMode());
}

void QnBusinessRulesDialog::updateAdvancedAction() {
    m_currentDetailsWidget->setVisible(advancedMode());
    m_advancedAction->setText(advancedMode() ? tr("Hide Advanced") : tr("Show Advanced"));
}

void QnBusinessRulesDialog::createActions() {
    m_newAction = new QAction(tr("&New..."), this);
    connect(m_newAction, SIGNAL(triggered()), this, SLOT(at_newRuleButton_clicked()));

    m_deleteAction = new QAction(tr("&Delete"), this);
    connect(m_deleteAction, SIGNAL(triggered()), this, SLOT(at_deleteButton_clicked()));

    m_advancedAction = new QAction(this);
    connect(m_advancedAction, SIGNAL(triggered()), this, SLOT(toggleAdvancedMode()));
    updateAdvancedAction();

    QAction* scheduleAct = new QAction(tr("&Schedule..."), this);
    connect(scheduleAct, SIGNAL(triggered()), m_currentDetailsWidget, SLOT(at_scheduleButton_clicked()));

    m_popupMenu->addAction(m_newAction);
    m_popupMenu->addAction(m_deleteAction);
    m_popupMenu->addSeparator();
    m_popupMenu->addAction(m_advancedAction);
    m_popupMenu->addAction(scheduleAct);
}

bool QnBusinessRulesDialog::saveAll() {
    QModelIndexList modified = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ModifiedRole, true, -1, Qt::MatchExactly);
    QModelIndexList invalid = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ValidRole, false, -1, Qt::MatchExactly);
    QSet<QModelIndex> invalid_modified = invalid.toSet().intersect(modified.toSet());

    if (!invalid_modified.isEmpty()) {
        QMessageBox::StandardButton btn =  QMessageBox::question(this,
                          tr("Confirm save"),
                          tr("Some rules are not valid. Should they be disabled?"),
                          QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                          QMessageBox::Cancel);

        switch (btn) {
        case QMessageBox::Yes:
            foreach (QModelIndex idx, invalid_modified) {
                m_rulesViewModel->getRuleModel(idx.row())->setDisabled(true);
            }
            break;
        case QMessageBox::No:
            break;
        default:
            return false;
        }
    }


    foreach (QModelIndex idx, modified) {
        m_rulesViewModel->saveRule(idx.row());
    }

    //TODO: #GDM replace with QnAppServerReplyProcessor
    foreach (const QnId& id, m_pendingDeleteRules) {
        int handle = QnAppServerConnectionFactory::getConnection2()->getBusinessEventManager()->deleteRule(
            id, this, &QnBusinessRulesDialog::at_resources_deleted );
        m_deleting[handle] = id;
    }
    m_pendingDeleteRules.clear();
    return true;
}

void QnBusinessRulesDialog::deleteRule(QnBusinessRuleViewModel* ruleModel) {
    if (ruleModel->id() > 0)
        m_pendingDeleteRules.append(ruleModel->id());
    m_rulesViewModel->deleteRule(ruleModel);
    updateControlButtons();
}

void QnBusinessRulesDialog::updateControlButtons() {
    bool hasRights = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;
    bool loaded = m_rulesViewModel->isLoaded();
    bool hasChanges = hasRights && loaded && (
                !m_rulesViewModel->match(m_rulesViewModel->index(0, 0), Qn::ModifiedRole, true, 1, Qt::MatchExactly).isEmpty()
             || !m_pendingDeleteRules.isEmpty()
                );
    bool canDelete = hasRights && loaded && m_currentDetailsWidget->model() && !m_currentDetailsWidget->model()->system();

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasRights && loaded);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasChanges);
    m_resetDefaultsButton->setEnabled(hasRights && loaded);

    ui->deleteRuleButton->setEnabled(canDelete);
    m_deleteAction->setEnabled(canDelete);

    ui->advancedButton->setEnabled(loaded && m_currentDetailsWidget->model());
    m_advancedAction->setEnabled(loaded && m_currentDetailsWidget->model());
    ui->addRuleButton->setEnabled(hasRights && loaded);
    m_newAction->setEnabled(hasRights && loaded);

    setAdvancedMode(hasRights && loaded && advancedMode());
}

bool isRuleVisible(QnBusinessRuleViewModel *ruleModel,
                   const QString &filter,
                   bool anyCameraPassFilter) {

    // system rules should never be displayed
    if (ruleModel->system())
        return false;

    // all rules shoud be visible if filter is empty
    if (filter.isEmpty())
        return true;

    if (BusinessEventType::requiresCameraResource(ruleModel->eventType())) {
        // rule supports any camera (assuming there is any camera that passing filter)
        if (ruleModel->eventResources().isEmpty() && anyCameraPassFilter)
            return true;

        // rule contains camera passing the filter
        foreach (const QnResourcePtr &resource, ruleModel->eventResources()) {
            if (resource->toSearchString().contains(filter, Qt::CaseInsensitive))
                return true;
        }
    }

    if (BusinessActionType::requiresCameraResource(ruleModel->actionType())) {
        foreach (const QnResourcePtr &resource, ruleModel->actionResources()) {
            if (resource->toSearchString().contains(filter, Qt::CaseInsensitive))
                return true;
        }
    }

    return false;

}

void QnBusinessRulesDialog::updateFilter() {
    QString filter = ui->filterLineEdit->text();
    /* Don't allow empty filters. */
    if (!filter.isEmpty() && filter.trimmed().isEmpty()) {
        ui->filterLineEdit->clear(); /* Will call into this slot again, so it is safe to return. */
        return;
    }

    ui->clearFilterButton->setVisible(!filter.isEmpty());

    if (!m_rulesViewModel->isLoaded())
        return;

    filter = filter.trimmed();
    bool anyCameraPassFilter = false;
    foreach (const QnResourcePtr camera, qnResPool->getAllEnabledCameras())  {
        anyCameraPassFilter = camera->toSearchString().contains(filter, Qt::CaseInsensitive);
        if (anyCameraPassFilter)
            break;
    }

    for (int i = 0; i < m_rulesViewModel->rowCount(); ++i) {
        QnBusinessRuleViewModel *ruleModel = m_rulesViewModel->getRuleModel(i);
        ui->tableView->setRowHidden(i, !isRuleVisible(ruleModel, filter, anyCameraPassFilter));
    }

}

bool QnBusinessRulesDialog::advancedMode() const {
    return m_advancedMode;
}

void QnBusinessRulesDialog::setAdvancedMode(bool value) {
    if (m_advancedMode == value)
        return;

    if (value && !m_currentDetailsWidget->model())
        return; // advanced options cannot be displayed

    m_advancedMode = value;
    updateAdvancedAction();
}
