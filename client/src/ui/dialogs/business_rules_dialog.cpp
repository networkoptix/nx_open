#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtCore/QEvent>

#include <QtGui/QMessageBox>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QItemEditorFactory>
#include <QtGui/QComboBox>
#include <QtGui/QPainter>
#include <QtGui/QKeyEvent>

#include <api/app_server_connection.h>

#include <utils/common/event_processors.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/resource.h>

#include <ui/delegate/business_rule_item_delegate.h>
#include <ui/dialogs/select_cameras_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/settings.h>

#include <client_message_processor.h>

QnBusinessRulesDialog::QnBusinessRulesDialog(QWidget *parent, QnWorkbenchContext *context):
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::BusinessRulesDialog()),
    m_popupMenu(new QMenu(this)),
    m_advancedAction(NULL),
    m_loadingHandle(-1)
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);
    m_currentDetailsWidget = ui->detailsWidget;

    createActions();

    m_rulesViewModel = new QnBusinessRulesViewModel(this, this->context());

    ui->tableView->setModel(m_rulesViewModel);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setResizeMode(QnBusiness::EventColumn, QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setResizeMode(QnBusiness::SourceColumn, QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setResizeMode(QnBusiness::ActionColumn, QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setResizeMode(QnBusiness::TargetColumn, QHeaderView::Interactive);

    ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
    ui->tableView->installEventFilter(this);

    ui->tableView->setItemDelegate(new QnBusinessRuleItemDelegate());

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


    //TODO: show description label if no rules are loaded

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(at_saveAllButton_clicked()));
    connect(ui->addRuleButton,                              SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));
    connect(ui->deleteRuleButton,                           SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));
    connect(ui->advancedButton,                             SIGNAL(clicked()), this, SLOT(at_advancedButton_clicked()));

    connect(context,  SIGNAL(userChanged(const QnUserResourcePtr &)),          this, SLOT(at_context_userChanged()));

    connect(QnClientMessageProcessor::instance(),           SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)),
            this, SLOT(at_message_ruleChanged(QnBusinessEventRulePtr)));
    connect(QnClientMessageProcessor::instance(),           SIGNAL(businessRuleDeleted(QnId)),
            this, SLOT(at_message_ruleDeleted(QnId)));

    at_context_userChanged();
    updateControlButtons();

    ui->horizontalSlider->setMaximum(1024);
    ui->horizontalSlider->setLockedValue(512);
    ui->horizontalSlider->setRecordedValue(600);
    ui->horizontalSlider->setValue(712);
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

void QnBusinessRulesDialog::accept()
{
    if (!saveAll())
        return;

    base_type::accept();
}

void QnBusinessRulesDialog::reject() {

    bool hasRights = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;
    bool loaded = m_loadingHandle < 0;
    bool hasChanges = hasRights && loaded && (
                !m_rulesViewModel->match(m_rulesViewModel->index(0, 0), QnBusiness::ModifiedRole, true, 1, Qt::MatchExactly).isEmpty()
             || !m_pendingDeleteRules.isEmpty()
                ); //TODO: calculate once and use anywhere
    if (!hasChanges) {
        base_type::reject();
        return;
    }

    QMessageBox::StandardButton btn =  QMessageBox::question(this,
                      tr("Confirm exit"),
                      tr("Unsaved changes will be lost. Save?"),
                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                      QMessageBox::Cancel);

    // TODO: #GDM codestyle, indentation. See how its done in other places for switch-case.
    switch (btn) {
        case QMessageBox::Yes:
            if (!saveAll())
                return;
            break;
        case QMessageBox::No:
            at_context_userChanged();
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

void QnBusinessRulesDialog::at_context_userChanged() {
   // bool enabled = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;
    m_currentDetailsWidget->setModel(NULL);

    m_rulesViewModel->clear();
    m_pendingDeleteRules.clear();
    m_deleting.clear();
    m_processing.clear();
    m_loadingHandle = -1;

    if ((accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        m_loadingHandle = QnAppServerConnectionFactory::createConnection()->getBusinessRulesAsync(
                    this, SLOT(at_resources_received(int,QByteArray,QnBusinessEventRules,int)));
    }

    updateControlButtons();
}

void QnBusinessRulesDialog::at_message_ruleChanged(const QnBusinessEventRulePtr &rule) {
    m_rulesViewModel->updateRule(rule);
    //TODO: ask user
}

void QnBusinessRulesDialog::at_message_ruleDeleted(QnId id) {
    m_rulesViewModel->deleteRule(id);
    foreach (QnBusinessEventRulePtr rule, m_pendingDeleteRules) {
        if (rule->getId() == id) {
            m_pendingDeleteRules.removeOne(rule);
            return;
        }
    }
    //TODO: ask user
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
    deleteRule(model);
}

void QnBusinessRulesDialog::at_advancedButton_clicked() {
    bool isAdvancedVisible = !m_currentDetailsWidget->isVisible() && m_currentDetailsWidget->model();
    m_currentDetailsWidget->setVisible(isAdvancedVisible);
    m_advancedAction->setText(isAdvancedVisible ? tr("Hide Advanced") : tr("Show Advanced"));
    //TODO: #GDM remove duplicate code
}

void QnBusinessRulesDialog::at_resources_received(int status, const QByteArray& errorString, const QnBusinessEventRules &rules, int handle) {

    if (handle != m_loadingHandle)
        return;

    bool success = (status == 0);
    if(!success) {
        //TODO: #GDM remove password from error message
        QMessageBox::critical(this, tr("Error while receiving rules"), QString::fromLatin1(errorString));
        return;
    }
    m_rulesViewModel->addRules(rules);
    m_loadingHandle = -1;

    ui->tableView->resizeColumnsToContents();
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
    updateControlButtons();
}

void QnBusinessRulesDialog::at_resources_saved(int status, const QByteArray& errorString, const QnBusinessEventRules &rules, int handle) {

    if (!m_processing.contains(handle))
        return;
    QnBusinessRuleViewModel* model = m_processing[handle];
    m_processing.remove(handle);

    bool success = (status == 0 && rules.size() == 1);
    if(!success) {
        //TODO: #GDM remove password from error message
        QMessageBox::critical(this, tr("Error while saving rule"), QString::fromLatin1(errorString));
        return;
    }

    QnBusinessEventRulePtr rule = rules.first();
    model->loadFromRule(rule); //here ID is set and modified flag is cleared
    updateControlButtons();
}

void QnBusinessRulesDialog::at_resources_deleted(const QnHTTPRawResponse& response, int handle) {
    if (!m_deleting.contains(handle))
        return;

    if(response.status != 0) {
        //TODO: #GDM remove password from error message
        QMessageBox::critical(this, tr("Error while deleting rule"), QString::fromLatin1(response.errorString));
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

void QnBusinessRulesDialog::createActions() {
    QAction* newAct = new QAction(tr("&New..."), this);
    connect(newAct, SIGNAL(triggered()), this, SLOT(at_newRuleButton_clicked()));

    QAction* deleteAct = new QAction(tr("&Delete"), this);
    connect(deleteAct, SIGNAL(triggered()), this, SLOT(at_deleteButton_clicked()));

    m_advancedAction = new QAction(this);
    connect(m_advancedAction, SIGNAL(triggered()), this, SLOT(at_advancedButton_clicked()));

    QAction* scheduleAct = new QAction(tr("&Schedule..."), this);
    connect(scheduleAct, SIGNAL(triggered()), m_currentDetailsWidget, SLOT(at_scheduleButton_clicked()));

    m_popupMenu->addAction(newAct);
    m_popupMenu->addAction(deleteAct);
    m_popupMenu->addSeparator();
    m_popupMenu->addAction(m_advancedAction);
    m_popupMenu->addAction(scheduleAct);
}

bool QnBusinessRulesDialog::saveAll() {
    QModelIndexList modified = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), QnBusiness::ModifiedRole, true, -1, Qt::MatchExactly);
    QModelIndexList invalid = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), QnBusiness::ValidRole, false, -1, Qt::MatchExactly);
    QSet<QModelIndex> invalid_modified = invalid.toSet().intersect(modified.toSet());

    if (!invalid_modified.isEmpty()) {
        QMessageBox::StandardButton btn =  QMessageBox::question(this,
                          tr("Confirm save invalid rules"),
                          tr("Some rules are not valid. Should we disable them?"),
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
        saveRule(m_rulesViewModel->getRuleModel(idx.row()));
    }
    foreach (QnBusinessEventRulePtr rule, m_pendingDeleteRules) {
        int handle = QnAppServerConnectionFactory::createConnection()->deleteAsync(rule, this, SLOT(at_resources_deleted(const QnHTTPRawResponse&, int)));
        m_deleting[handle] = rule;
    }
    m_pendingDeleteRules.clear();
    return true;
}

void QnBusinessRulesDialog::saveRule(QnBusinessRuleViewModel* ruleModel) {
    if (m_processing.values().contains(ruleModel))
        return;

    QnBusinessEventRulePtr rule = ruleModel->createRule();
    int handle = QnAppServerConnectionFactory::createConnection()->saveAsync(
                rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnBusinessEventRules &, int)));
    m_processing[handle] = ruleModel;
}

void QnBusinessRulesDialog::deleteRule(QnBusinessRuleViewModel* ruleModel) {
    if (ruleModel->id()) {
        m_pendingDeleteRules.append(ruleModel->createRule());
    }
    m_rulesViewModel->deleteRule(ruleModel);
    updateControlButtons();
}

void QnBusinessRulesDialog::updateControlButtons() {
    bool hasRights = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;
    bool loaded = m_loadingHandle < 0;
    bool hasChanges = hasRights && loaded && (
                !m_rulesViewModel->match(m_rulesViewModel->index(0, 0), QnBusiness::ModifiedRole, true, 1, Qt::MatchExactly).isEmpty()
             || !m_pendingDeleteRules.isEmpty()
                );

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasRights && loaded);

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasChanges);

    ui->deleteRuleButton->setEnabled(hasRights && loaded && m_currentDetailsWidget->model());

    ui->advancedButton->setEnabled(loaded && m_currentDetailsWidget->model());
    m_advancedAction->setEnabled(loaded && m_currentDetailsWidget->model());

    bool isAdvancedVisible = m_currentDetailsWidget->isVisible() & loaded && m_currentDetailsWidget->model();
    m_currentDetailsWidget->setVisible(isAdvancedVisible);
    m_advancedAction->setText(isAdvancedVisible ? tr("Hide Advanced") : tr("Show Advanced"));

    ui->addRuleButton->setEnabled(hasRights && loaded);
}
