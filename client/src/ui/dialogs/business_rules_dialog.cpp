#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"
#include <ui/widgets/business/business_rule_widget.h>

#include <api/app_server_connection.h>

QnBusinessRulesDialog::QnBusinessRulesDialog(QnAppServerConnectionPtr connection, QWidget *parent):
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::BusinessRulesDialog()),
    m_connection(connection)
{
    ui->setupUi(this);

    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(ui->newRuleButton, SIGNAL(clicked()), this, SLOT(at_newRuleButton_clicked()));

    QnBusinessRuleWidget *w1 = new QnBusinessRuleWidget(this);
    QnBusinessRuleWidget *w2 = new QnBusinessRuleWidget(this);

    ui->verticalLayout->insertWidget(0, w2);
    ui->verticalLayout->insertWidget(0, w1);
    w2->setExpanded(true);
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}

void QnBusinessRulesDialog::at_newRuleButton_clicked() {
    QnBusinessEventRulePtr rule(new QnBusinessEventRule());
    m_connection->saveAsync(rule, this, SLOT(at_resources_saved(int, const QByteArray &, const QnResourceList &, int)));
}



