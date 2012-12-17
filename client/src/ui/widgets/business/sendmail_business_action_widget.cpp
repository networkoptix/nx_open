#include "sendmail_business_action_widget.h"
#include "ui_sendmail_business_action_widget.h"

#include <events/sendmail_business_action.h>

QnSendmailBusinessActionWidget::QnSendmailBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnSendmailBusinessActionWidget)
{
    ui->setupUi(this);
}

QnSendmailBusinessActionWidget::~QnSendmailBusinessActionWidget()
{
    delete ui;
}

void QnSendmailBusinessActionWidget::loadParameters(const QnBusinessParams &params) {
    ui->emailLineEdit->setText(BusinessActionParameters::getEmailAddress(params));
}

QnBusinessParams QnSendmailBusinessActionWidget::parameters() const {
    QnBusinessParams params;
    BusinessActionParameters::setEmailAddress(&params, ui->emailLineEdit->text());
    return params;
}
