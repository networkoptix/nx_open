#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

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
    connect(ui->newRuleButton, SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));
    connect(ui->newRuleWidget, SIGNAL(apply(QnBusinessRuleWidget*, QnBusinessEventRulePtr)), this, SLOT(newRule(QnBusinessRuleWidget*, QnBusinessEventRulePtr)));
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    ui->newRuleWidget->setExpanded(!ui->newRuleWidget->expanded());
}

void QnBusinessRulesDialog::at_resources_saved(int status, const QByteArray& errorString, const QnResourceList &resources, int handle) {
    Q_UNUSED(resources)

    if (!m_savingWidgets.contains(handle))
        return;
    QWidget* w = m_savingWidgets[handle];
    w->setEnabled(true);
    m_savingWidgets.remove(handle);

    qDebug() << "saving rule...";
    if(status != 0)
        qDebug() << errorString;
    else
        qDebug() << "success";
    //TODO: update rule caption
}

void QnBusinessRulesDialog::at_resources_deleted(const QnHTTPRawResponse& response, int handle) {
    if (!m_deletingWidgets.contains(handle))
        return;
    QWidget* w = m_deletingWidgets[handle];
    ui->verticalLayout->removeWidget(w);
    w->setVisible(false);
    m_deletingWidgets.remove(handle);

    qDebug() << "deleting rule...";
    if(response.status != 0)
        qDebug() << response.errorString;
    else
        qDebug() << "success";
    //TODO: remove rule from the list
}

void QnBusinessRulesDialog::addRuleToList(QnBusinessEventRulePtr rule) {
    QnBusinessRuleWidget *w = new QnBusinessRuleWidget(this);
    w->setRule(rule);
    connect(w, SIGNAL(apply(QnBusinessRuleWidget*, QnBusinessEventRulePtr)), this, SLOT(saveRule(QnBusinessRuleWidget*, QnBusinessEventRulePtr)));
    connect(w, SIGNAL(deleteConfirmed(QnBusinessRuleWidget*, QnBusinessEventRulePtr)), this, SLOT(deleteRule(QnBusinessRuleWidget*, QnBusinessEventRulePtr)));
    ui->verticalLayout->insertWidget(0, w);
}


void QnBusinessRulesDialog::newRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule) {
    saveRule(widget, rule);
    addRuleToList(rule);
}

void QnBusinessRulesDialog::saveRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule) {
    if (m_savingWidgets.values().contains(widget))
        return;

    int handle = m_connection->saveAsync(rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
    m_savingWidgets[handle] = widget;
    widget->setEnabled(false);
    //TODO: rule caption should be modified with "Saving..."
}

void QnBusinessRulesDialog::deleteRule(QnBusinessRuleWidget* widget, QnBusinessEventRulePtr rule) {
    if (m_deletingWidgets.values().contains(widget))
        return;

    int handle = m_connection->deleteAsync(rule, this, SLOT(at_resources_deleted(const QnHTTPRawResponse&, int)));
    m_deletingWidgets[handle] = widget;
    widget->setEnabled(false);
    //TODO: rule caption should be modified with "Removing..."
}
