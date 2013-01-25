#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QItemEditorFactory>
#include <QtGui/QComboBox>
#include <QtGui/QPainter>

#include <api/app_server_connection.h>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/resource.h>

#include <ui/delegate/business_rule_item_delegate.h>
#include <ui/dialogs/select_cameras_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include "ui/workbench/workbench_access_controller.h"

#include <utils/settings.h>

#include <client_message_processor.h>

namespace {
    class QnAbstractComboBoxEditorFactory: public QItemEditorFactory {

    protected:
        virtual void populateComboBox(QComboBox* comboBox) const = 0;
    public:
        virtual QWidget *createEditor(QVariant::Type type, QWidget *parent) const override {
            Q_UNUSED(type)
            QComboBox* result = new QComboBox(parent);
            populateComboBox(result);
            return result;
        }

        virtual QByteArray valuePropertyName(QVariant::Type type) const override {
            Q_UNUSED(type)
            return QByteArray("currentIndex");
        }
    };

    class QnBusinessEventTypeEditorFactory: public QnAbstractComboBoxEditorFactory {
    protected:
        virtual void populateComboBox(QComboBox *comboBox) const override {
            for (int i = 0; i < BusinessEventType::BE_Count; i++) {
                BusinessEventType::Value val = (BusinessEventType::Value)i;

                comboBox->insertItem(i, BusinessEventType::toString(val));
                comboBox->setItemData(i, val);
            }
        }
    };

    class QnBusinessActionTypeEditorFactory: public QnAbstractComboBoxEditorFactory {
    protected:
        virtual void populateComboBox(QComboBox *comboBox) const override {
            for (int i = 0; i < BusinessActionType::BA_Count; i++) {
                BusinessActionType::Value val = (BusinessActionType::Value)i;

                comboBox->insertItem(i, BusinessActionType::toString(val));
                comboBox->setItemData(i, val);
            }
        }
    };

}

QnBusinessRulesDialog::QnBusinessRulesDialog(QWidget *parent, QnWorkbenchContext *context):
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::BusinessRulesDialog()),
    m_loadingHandle(-1)
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);

    m_rulesViewModel = new QnBusinessRulesViewModel(this, this->context());

    m_currentDetailsWidget = new QnBusinessRuleWidget(this, this->context());
    ui->detailsLayout->addWidget(m_currentDetailsWidget);

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

    QStyledItemDelegate *eventTypeItemDelegate = new QnBusinessRuleItemDelegate();
    eventTypeItemDelegate->setItemEditorFactory(new QnBusinessEventTypeEditorFactory());
    ui->tableView->setItemDelegateForColumn(QnBusiness::EventColumn, eventTypeItemDelegate);

    QStyledItemDelegate *actionTypeItemDelegate = new QnBusinessRuleItemDelegate();
    actionTypeItemDelegate->setItemEditorFactory(new QnBusinessActionTypeEditorFactory());
    ui->tableView->setItemDelegateForColumn(QnBusiness::ActionColumn, actionTypeItemDelegate);

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
    connect(ui->advancedButton,                             SIGNAL(clicked()), this, SLOT(at_advancedButton_clicked()));

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
    m_currentDetailsWidget->setModel(NULL);

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
    //TODO: ask user
}

void QnBusinessRulesDialog::at_message_ruleDeleted(QnId id) {
    m_rulesViewModel->deleteRule(id);
    //TODO: ask user
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    m_rulesViewModel->addRule(QnBusinessEventRulePtr());
    ui->tableView->setCurrentIndex(m_rulesViewModel->index(m_rulesViewModel->rowCount() - 1, 0));
}

void QnBusinessRulesDialog::at_saveAllButton_clicked() {
    QModelIndexList modified = m_rulesViewModel->match(m_rulesViewModel->index(0, 0), QnBusiness::ModifiedRole, true, -1, Qt::MatchExactly);
    foreach (QModelIndex idx, modified) {
        saveRule(m_rulesViewModel->getRuleModel(idx.row()));
    }
}

void QnBusinessRulesDialog::at_deleteButton_clicked() {
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

void QnBusinessRulesDialog::at_advancedButton_clicked() {
    ui->detailsFrame->setVisible(!ui->detailsFrame->isVisible());
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
    m_currentDetailsWidget->setModel(ruleModel);

    updateControlButtons();
}

void QnBusinessRulesDialog::at_model_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    Q_UNUSED(bottomRight)
    if (topLeft.column() <= QnBusiness::ModifiedColumn && bottomRight.column() >= QnBusiness::ModifiedColumn)
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

    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasRights && loaded &&
       !m_rulesViewModel->match(m_rulesViewModel->index(0, 0), QnBusiness::ModifiedRole, true, 1, Qt::MatchExactly).isEmpty());

    ui->deleteRuleButton->setEnabled(hasRights && loaded && m_currentDetailsWidget->model());

    ui->advancedButton->setEnabled(loaded && m_currentDetailsWidget->model());
    ui->detailsFrame->setVisible(ui->detailsFrame->isVisible() & loaded && m_currentDetailsWidget->model());

    ui->addRuleButton->setEnabled(hasRights && loaded);
}
