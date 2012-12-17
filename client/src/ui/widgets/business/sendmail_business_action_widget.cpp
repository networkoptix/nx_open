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

QString QnSendmailBusinessActionWidget::description() const {
    QString fmt = QLatin1String("%1 <span style=\"font-style:italic;\">%2</span>");
    QString recordStr = QObject::tr("E-mail to");
    return fmt
            .arg(recordStr)
            .arg(ui->emailLineEdit->text());
}
