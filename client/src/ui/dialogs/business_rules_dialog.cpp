#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"
#include <ui/widgets/business/business_rule_widget.h>

#include <api/app_server_connection.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/resource.h>

QnBusinessRulesDialog::QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent):
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::BusinessRulesDialog()),
    m_connection(connection)
{
    ui->setupUi(this);

    QnBusinessEventRules rules;
    QByteArray errString;
    connection->getBusinessRules(rules, errString); //sync :(
    foreach (QnBusinessEventRulePtr rule, rules) {
        addRuleToList(rule);
    }

    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(ui->newRuleButton, SIGNAL(clicked()), ui->newRuleWidget, SIGNAL(expand()));
    connect(ui->newRuleWidget, SIGNAL(apply(QnBusinessEventRulePtr)), this, SLOT(saveRule(QnBusinessEventRulePtr)));
    connect(ui->newRuleWidget, SIGNAL(apply(QnBusinessEventRulePtr)), this, SLOT(addRuleToList(QnBusinessEventRulePtr)));

}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    //TODO: just expand "new rule" widget without short caption and Reset button
    QnBusinessEventRulePtr rule(new QnBusinessEventRule());
    rule->setActionType(BusinessActionType::BA_SendMail);
    rule->setEventType(BusinessEventType::BE_Camera_Input);
    m_connection->saveAsync(rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

void QnBusinessRulesDialog::at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {
    Q_UNUSED(handle)
    Q_UNUSED(resources)

    qDebug() << "saving rule...";
    if(status == 0)
        qDebug() << errorString;
    else
        qDebug() << "success";
}

void QnBusinessRulesDialog::addRuleToList(QnBusinessEventRulePtr rule) {
    QnBusinessRuleWidget *w = new QnBusinessRuleWidget(this);
    w->setRule(rule);
    connect(w, SIGNAL(apply(QnBusinessEventRulePtr)), this, SLOT(saveRule(QnBusinessEventRulePtr)));
    ui->verticalLayout->insertWidget(0, w);
}

void QnBusinessRulesDialog::saveRule(QnBusinessEventRulePtr rule) {
    m_connection->saveAsync(rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}

