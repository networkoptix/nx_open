#include "business_rules_dialog.h"
#include "ui_business_rules_dialog.h"

#include <ui/widgets/business_rule_widget.h>


QnBusinessRulesDialog::QnBusinessRulesDialog(QWidget *parent): 
    QDialog(parent, Qt::MSWindowsFixedSizeDialogHint),
    ui(new Ui::BusinessRulesDialog())
{
    ui->setupUi(this);

    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QnBusinessRuleWidget *w1 = new QnBusinessRuleWidget(this);
    QnBusinessRuleWidget *w2 = new QnBusinessRuleWidget(this);

    ui->verticalLayout->insertWidget(0, w2);
    ui->verticalLayout->insertWidget(0, w1);
    w2->setExpanded(true);
}

QnBusinessRulesDialog::~QnBusinessRulesDialog()
{
}





