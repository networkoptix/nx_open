#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnBusinessRuleWidget)
{
    ui->setupUi(this);
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{
    delete ui;
}
