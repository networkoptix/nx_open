#include "sendmail_business_action_widget.h"
#include "ui_sendmail_business_action_widget.h"

#include <events/sendmail_business_action.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

QnSendmailBusinessActionWidget::QnSendmailBusinessActionWidget(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnSendmailBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->emailLineEdit, SIGNAL(textChanged(QString)), this, SIGNAL(parametersChanged()));
    connect(ui->settingsButton, SIGNAL(clicked()), this, SLOT(at_settingsButton_clicked()));
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
    return ui->emailLineEdit->text();
}

void QnSendmailBusinessActionWidget::at_settingsButton_clicked() {
    menu()->trigger(Qn::OpenServerSettingsAction);
}
