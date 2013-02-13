#include "sendmail_business_action_widget.h"
#include "ui_sendmail_business_action_widget.h"

#include <business/actions/sendmail_business_action.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

QnSendmailBusinessActionWidget::QnSendmailBusinessActionWidget(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::QnSendmailBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->emailLineEdit, SIGNAL(textChanged(QString)), this, SLOT(paramsChanged()));
    connect(ui->settingsButton, SIGNAL(clicked()), this, SLOT(at_settingsButton_clicked()));
}

QnSendmailBusinessActionWidget::~QnSendmailBusinessActionWidget()
{
}

void QnSendmailBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QnScopedValueRollback<bool> guard(&m_updating, true);
    Q_UNUSED(guard)

    if (fields & QnBusiness::ActionParamsField) {
        QString email = BusinessActionParameters::getEmailAddress(model->actionParams());
        if (ui->emailLineEdit->text() != email)
            ui->emailLineEdit->setText(email);
    }
}

void QnSendmailBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessParams params;
    BusinessActionParameters::setEmailAddress(&params, ui->emailLineEdit->text());
    model()->setActionParams(params);
}


void QnSendmailBusinessActionWidget::at_settingsButton_clicked() {
    menu()->trigger(Qn::OpenServerSettingsAction);
}
