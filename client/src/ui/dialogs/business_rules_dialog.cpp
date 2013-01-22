#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtGui/QMessageBox>

#include <api/app_server_connection.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/resource.h>

#include <ui/style/resource_icon_cache.h>

#include <ui/workbench/workbench_context.h>
#include "ui/workbench/workbench_access_controller.h"

#include <utils/settings.h>

#include <client_message_processor.h>

QnBusinessRulesDialog::QnBusinessRulesDialog(QWidget *parent, QnWorkbenchContext *context):
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::BusinessRulesDialog()),
    m_currentDetailsWidget(NULL),
    m_loadingHandle(-1)
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);

    m_rulesViewModel = new QnBusinessRulesViewModel(this, this->context());

    ui->tableView->setModel(m_rulesViewModel);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->installEventFilter(this);

    connect(m_rulesViewModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_model_dataChanged(QModelIndex,QModelIndex)));
    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));

    ui->tableView->clearSelection();

    //TODO: show description label if no rules are loaded

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(at_saveAllButton_clicked()));
    connect(ui->buttonBox,                                  SIGNAL(accepted()),this, SLOT(at_saveAllButton_clicked()));
    connect(ui->addRuleButton,                              SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));
    connect(ui->deleteRuleButton,                           SIGNAL(clicked()), this, SLOT(at_deleteButton_clicked()));

    connect(context,  SIGNAL(userChanged(const QnUserResourcePtr &)),          this, SLOT(at_context_userChanged()));

    connect(QnClientMessageProcessor::instance(),           SIGNAL(businessRuleChanged(QnBusinessEventRulePtr)),
            this, SLOT(at_message_ruleChanged(QnBusinessEventRulePtr)));
    connect(QnClientMessageProcessor::instance(),           SIGNAL(businessRuleDeleted(QnId)),
            this, SLOT(at_message_ruleDeleted(QnId)));


//    connect(ui->closeButton,    SIGNAL(clicked()), this, SLOT(reject()));

    at_context_userChanged();
    updateControlButtons();
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

bool QnBusinessRulesDialog::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
        if (pKeyEvent->key() == Qt::Key_Delete) {
            at_deleteButton_clicked();
            return true;
        }
    }
    return base_type::eventFilter(object, event);
}

void QnBusinessRulesDialog::at_context_userChanged() {
   // bool enabled = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;

    if (m_currentDetailsWidget) {
        ui->detailsLayout->removeWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(false);
        m_currentDetailsWidget = NULL;
    }

    m_rulesViewModel->clear();
    m_loadingHandle = -1;

    if ((accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        m_loadingHandle = QnAppServerConnectionFactory::createConnection()->getBusinessRulesAsync(
                    this, SLOT(at_resources_received(int,QByteArray,QnBusinessEventRules,int)));
    }

    updateControlButtons();
}

void QnBusinessRulesDialog::at_message_ruleChanged(const QnBusinessEventRulePtr &rule) {
    m_rulesViewModel->updateRule(rule);
    //widget by rule, item by widget - already written
    qDebug() << "rule changed" << rule->getId();
    //TODO: ask user
}

void QnBusinessRulesDialog::at_message_ruleDeleted(QnId id) {
    m_rulesViewModel->deleteRule(id);
    //widget by rule, item by widget - already written
    qDebug() << "rule deleted" << id;
    //TODO: ask user
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    m_rulesViewModel->addRule(QnBusinessEventRulePtr());
    ui->tableView->setCurrentIndex(m_rulesViewModel->index(m_rulesViewModel->rowCount() - 1, 0));
}

void QnBusinessRulesDialog::at_saveAllButton_clicked() {
    for (int i = 0; i < m_rulesViewModel->rowCount(); i++) {
        QnBusinessRuleViewModel* rule = m_rulesViewModel->getRuleModel(i);
        if (!rule->isModified())
            continue;
        saveRule(rule);
    }
}

void QnBusinessRulesDialog::at_deleteButton_clicked() {
    if (!m_currentDetailsWidget)
        return;

    QnBusinessRuleViewModel* model = m_currentDetailsWidget->model();
    if (!model)
        return;

    if (model->id() &&
            QMessageBox::question(this,
                              tr("Confirm rule deletion"),
                              tr("Are you sure you want to delete this rule?"),
                              QMessageBox::Ok,
                              QMessageBox::Cancel) == QMessageBox::Cancel)
        return;

    deleteRule(model);
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
    model->loadFromRule(rule);
    updateControlButtons();
}

void QnBusinessRulesDialog::at_resources_deleted(const QnHTTPRawResponse& response, int handle) {

    if (!m_processing.contains(handle))
        return;
    QnBusinessRuleViewModel* model = m_processing[handle];
    m_processing.remove(handle);

    if(response.status != 0) {
        //TODO: #GDM remove password from error message
        QMessageBox::critical(this, tr("Error while deleting rule"), QString::fromLatin1(response.errorString));
        return;
    }

    m_rulesViewModel->deleteRule(model);
    updateControlButtons();
}

void QnBusinessRulesDialog::at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous)

    QnBusinessRuleViewModel* ruleModel = m_rulesViewModel->getRuleModel(current.row());
    if (!m_currentDetailsWidget) {
        m_currentDetailsWidget = new QnBusinessRuleWidget(this, context());
        ui->detailsLayout->addWidget(m_currentDetailsWidget);
    }
    m_currentDetailsWidget->setModel(ruleModel);

    updateControlButtons();
}

void QnBusinessRulesDialog::at_model_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    Q_UNUSED(bottomRight)
    if (topLeft.column() <= QnBusiness::ModifiedColumn)
        updateControlButtons();
}

void QnBusinessRulesDialog::saveRule(QnBusinessRuleViewModel* ruleModel) {
    if (m_processing.values().contains(ruleModel))
        return;
    //TODO: set rule status to "Saving"

    QnBusinessEventRulePtr rule = ruleModel->createRule();
    int handle = QnAppServerConnectionFactory::createConnection()->saveAsync(
                rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnBusinessEventRules &, int)));
    m_processing[handle] = ruleModel;
}

void QnBusinessRulesDialog::deleteRule(QnBusinessRuleViewModel* ruleModel) {
    if (m_processing.values().contains(ruleModel))
        return;

    if (!ruleModel->id()) {
        m_rulesViewModel->deleteRule(ruleModel);
        return;
    }

    QnBusinessEventRulePtr rule = ruleModel->createRule();
    int handle = QnAppServerConnectionFactory::createConnection()->deleteAsync(
                rule, this, SLOT(at_resources_deleted(const QnHTTPRawResponse&, int)));
    m_processing[handle] = ruleModel;

    //TODO: rule status should be set to "Removing..."
}

void QnBusinessRulesDialog::updateControlButtons() {
    bool hasRights = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;
    bool loaded = m_loadingHandle < 0;

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasRights && loaded);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasRights && loaded && m_rulesViewModel->hasModifiedItems());

    ui->deleteRuleButton->setEnabled(hasRights && loaded && m_currentDetailsWidget);
    ui->addRuleButton->setEnabled(hasRights && loaded);
}
