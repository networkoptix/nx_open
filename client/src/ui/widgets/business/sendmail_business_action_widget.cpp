#include "sendmail_business_action_widget.h"
#include "ui_sendmail_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <ui/actions/action_manager.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

QnSendmailBusinessActionWidget::QnSendmailBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SendmailBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->emailLineEdit, SIGNAL(textChanged(QString)), this, SLOT(paramsChanged()));
    connect(ui->settingsButton, &QPushButton::clicked, action(Qn::PreferencesSmtpTabAction), &QAction::trigger);
}

QnSendmailBusinessActionWidget::~QnSendmailBusinessActionWidget()
{
}

void QnSendmailBusinessActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->emailLineEdit);
    setTabOrder(ui->emailLineEdit, ui->settingsButton);
    setTabOrder(ui->settingsButton, after);
}

void QnSendmailBusinessActionWidget::at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) {
    if (!model)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::ActionParamsField) {
        QString email = (model->actionParams().emailAddress);
        if (ui->emailLineEdit->text() != email)
            ui->emailLineEdit->setText(email);
    }
}

void QnSendmailBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QnBusinessActionParameters params;
    params.emailAddress = ui->emailLineEdit->text();
    model()->setActionParams(params);
}
