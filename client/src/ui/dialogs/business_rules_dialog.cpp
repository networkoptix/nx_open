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

namespace {
    QnBusinessEventRulePtr ruleById(QnBusinessEventRules rules, QString uniqId) {
        foreach(const QnBusinessEventRulePtr& rule, rules)
            if (rule->getUniqueId() == uniqId)
                return rule;
        return QnBusinessEventRulePtr();
    }

}

QnBusinessRulesDialog::QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent, QnWorkbenchContext *context):
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::BusinessRulesDialog()),
    m_currentDetailsWidget(NULL),
    m_connection(connection)
{
    ui->setupUi(this);
    setButtonBox(ui->buttonBox);

    m_rulesViewModel = new QnBusinessRulesViewModel(this, this->context());

    ui->tableView->setModel(m_rulesViewModel);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->installEventFilter(this);

    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));

    //ui->tableView->resizeColumnsToContents();
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

    if ((accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        QnBusinessEventRules rules;
        m_connection->getBusinessRules(rules); // TODO: replace synchronous call
        m_rulesViewModel->addRules(rules);
       /* foreach (QnBusinessEventRulePtr rule, rules) {
            QnBusinessRuleWidget* w = createWidget(rule);
            m_listModel->appendRow(createRow(w));
            w->resetFromRule(); //here row data will be updated
        }*/
    }

    updateControlButtons();
}

void QnBusinessRulesDialog::at_message_ruleChanged(const QnBusinessEventRulePtr &rule) {
    //widget by rule, item by widget - already written
    qDebug() << "rule changed" << rule->getId();
}

void QnBusinessRulesDialog::at_message_ruleDeleted(QnId id) {
    //widget by rule, item by widget - already written
    qDebug() << "rule deleted" << id;
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
   /* QnBusinessEventRulePtr rule = QnBusinessEventRulePtr(new QnBusinessEventRule());
    //TODO: wizard dialog?
    rule->setEventType(BusinessEventType::BE_Camera_Disconnect);
    rule->setActionType(BusinessActionType::BA_Alert);

    QnBusinessRuleWidget* w = createWidget(rule);
    m_listModel->appendRow(createRow(w));
    w->resetFromRule(); //here row data will be updated
    w->setHasChanges(true);

    ui->tableView->selectRow(m_listModel->rowCount() - 1);*/
}

void QnBusinessRulesDialog::at_saveAllButton_clicked() {
  /*  for (int i = 0; i < m_listModel->rowCount(); i++) {
        QStandardItem* item = m_listModel->item(i);
        QnBusinessRuleWidget* w = (QnBusinessRuleWidget *)item->data(WidgetRole).value<QWidget *>();
        if (!w || !w->hasChanges())
            continue;
        saveRule(w);
    }*/
}

void QnBusinessRulesDialog::at_deleteButton_clicked() {
  /*  if (!m_currentDetailsWidget && !m_currentDetailsWidget->hasChanges())
        return;

    if (m_currentDetailsWidget->rule()->getId() &&
            QMessageBox::question(this,
                              tr("Confirm rule deletion"),
                              tr("Are you sure you want to delete this rule?"),
                              QMessageBox::Ok,
                              QMessageBox::Cancel) == QMessageBox::Cancel)
        return;

    deleteRule(m_currentDetailsWidget);*/
}

void QnBusinessRulesDialog::at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {

    /*if (!m_processingWidgets.contains(handle))
        return;

    QnBusinessRuleWidget* w = m_processingWidgets[handle];
    w->setEnabled(true);
    m_processingWidgets.remove(handle);

    bool success = (status == 0 && resources.size() == 1);
    if(!success) {
        QMessageBox::critical(this, tr("Error while saving rule"), QString::fromLatin1(errorString));
        return;
    }
    //w->setHasChanges(false);

    //TODO: load changes from resource

    QnResourcePtr res = resources.first();
    QnBusinessEventRulePtr rule = res.dynamicCast<QnBusinessEventRule>();
    if (!rule)
        return;

    w->rule()->setId(rule->getId());
    w->resetFromRule();
    updateControlButtons();*/
}

void QnBusinessRulesDialog::at_resources_deleted(const QnHTTPRawResponse& response, int handle) {

  /*  if (!m_processingWidgets.contains(handle))
        return;
    QnBusinessRuleWidget* w = m_processingWidgets[handle];
    w->setEnabled(true);
    m_processingWidgets.remove(handle);

    if(response.status != 0) {
        //TODO: #GDM remove password from error message
        QMessageBox::critical(this, tr("Error while deleting rule"), QString::fromLatin1(response.errorString));
        return;
    }

    updateControlButtons();

    QModelIndexList ruleIdx = m_listModel->match(m_listModel->index(0, 0), WidgetRole,
                                                 QVariant::fromValue<QWidget *>(w),
                                                 1, Qt::MatchExactly);
    if (ruleIdx.isEmpty())
        return;
    int row = ruleIdx.first().row();
    m_listModel->removeRow(row);

    if (m_currentDetailsWidget == w) {
        ui->detailsLayout->removeWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(false);
        m_currentDetailsWidget = NULL;
        ui->tableView->clearSelection();
    }

    delete w;*/
}

/*void QnBusinessRulesDialog::at_widgetActionResourcesChanged(QnBusinessRuleWidget* source, BusinessActionType::Value actionType, const QnResourceList &resources) {
    QStandardItem *item = tableItem(source, 5);

    if (actionType == BusinessActionType::BA_SendMail) {
        QString recipients = source->actionResourcesText();
        QStringList list = recipients.split(QLatin1Char(';'), QString::SkipEmptyParts);

        switch (list.size()){
            case 0:
                item->setIcon(qnResIconCache->icon(QnResourceIconCache::Offline, true));
                item->setText(tr("<Enter at least one address>"));
                break;
            case 1:
                item->setIcon(qnResIconCache->icon(QnResourceIconCache::User));
                item->setText(recipients);
                break;
            default:
                item->setIcon(qnResIconCache->icon(QnResourceIconCache::Users));
                item->setText(recipients);
                break;
        }
    }
    else if (!BusinessActionType::isResourceRequired(actionType)) {
        item->setIcon(qnResIconCache->icon(QnResourceIconCache::Servers));
        item->setText(tr("<System>"));
    } else if (resources.size() == 1) {
        QnResourcePtr resource = resources.first();
        item->setIcon(qnResIconCache->icon(resource->flags(), resource->getStatus()));
        item->setText(getResourceName(resource));
    } else {
        //TODO: #GDM popup action will require user resources
        if (resources.size() == 0) {
            item->setIcon(qnResIconCache->icon(QnResourceIconCache::Offline, true));
            item->setText(tr("<Select at least one camera>"));
        }
        else {
            item->setIcon(qnResIconCache->icon(QnResourceIconCache::Camera));
            item->setText(tr("%1 Cameras").arg(resources.size())); //TODO: fix tr to %n
        }
    }
}*/

void QnBusinessRulesDialog::at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous)
/*
    if (m_currentDetailsWidget) {
        ui->detailsLayout->removeWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(false);
        m_currentDetailsWidget = NULL;
    }

    QStandardItem* item = m_listModel->itemFromIndex(current.sibling(current.row(), 0));
    m_currentDetailsWidget = (QnBusinessRuleWidget *)item->data(WidgetRole).value<QWidget *>();

    if (m_currentDetailsWidget) {
        ui->detailsLayout->addWidget(m_currentDetailsWidget);
        m_currentDetailsWidget->setVisible(true);
    }
*/
    QnBusinessRuleViewModel* ruleModel = m_rulesViewModel->getRuleModel(current.row());
    if (!m_currentDetailsWidget) {
        m_currentDetailsWidget = new QnBusinessRuleWidget(this, context());
        ui->detailsLayout->addWidget(m_currentDetailsWidget);
    }
    m_currentDetailsWidget->setModel(ruleModel);

    updateControlButtons();
}

void QnBusinessRulesDialog::saveRule(QnBusinessRuleWidget* widget) {
  /*  if (m_processingWidgets.values().contains(widget))
        return;

    //save changes to the rule field
    widget->apply();

    QnBusinessEventRulePtr rule = widget->rule();
    int handle = m_connection->saveAsync(rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    widget->setEnabled(false);
    m_processingWidgets[handle] = widget;
    //TODO: update row
    //TODO: rule caption should be modified with "Saving..."*/
}

void QnBusinessRulesDialog::deleteRule(QnBusinessRuleWidget* widget) {
 /*   if (m_processingWidgets.values().contains(widget))
        return;

    QnBusinessEventRulePtr rule = widget->rule();
    if (!rule->getId()) {
        QModelIndexList ruleIdx = m_listModel->match(m_listModel->index(0, 0), WidgetRole,
                                                     QVariant::fromValue<QWidget *>(widget),
                                                     1, Qt::MatchExactly);
        if (!ruleIdx.isEmpty()) {
            int rowNum = ruleIdx.first().row();
            m_listModel->removeRow(rowNum);
            ui->tableView->clearSelection();
        }
        return;
    }

    int handle = m_connection->deleteAsync(rule, this, SLOT(at_resources_deleted(const QnHTTPRawResponse&, int)));
    widget->setEnabled(false);
    m_processingWidgets[handle] = widget;

    //TODO: rule caption should be modified with "Removing..."*/
}

void QnBusinessRulesDialog::updateControlButtons() {
 /*   bool hasRights = accessController()->globalPermissions() & Qn::GlobalProtectedPermission;

//    ui->saveButton->setEnabled(m_currentDetailsWidget && m_currentDetailsWidget->hasChanges());
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasRights);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(hasRights &&
         !m_listModel->match(m_listModel->index(0, 0), ModifiedRole, true, 1, Qt::MatchExactly).isEmpty());

    ui->deleteRuleButton->setEnabled(hasRights && m_currentDetailsWidget);
    ui->addRuleButton->setEnabled(hasRights);*/
}
