#include "sendmail_business_action_widget.h"
#include "ui_sendmail_business_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <QtWidgets/QAction>

#include <nx/vms/event/action_parameters.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop::ui;

QnSendmailBusinessActionWidget::QnSendmailBusinessActionWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SendmailBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->emailLineEdit, &QLineEdit::textChanged,
        this, &QnSendmailBusinessActionWidget::paramsChanged);

    connect(ui->settingsButton, &QPushButton::clicked,
        action(action::PreferencesSmtpTabAction), &QAction::trigger);
}

QnSendmailBusinessActionWidget::~QnSendmailBusinessActionWidget()
{
}

void QnSendmailBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->emailLineEdit);
    setTabOrder(ui->emailLineEdit, ui->settingsButton);
    setTabOrder(ui->settingsButton, after);
}

void QnSendmailBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model())
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::actionParams))
    {
        QString email = (model()->actionParams().emailAddress);
        if (ui->emailLineEdit->text() != email)
            ui->emailLineEdit->setText(email);
    }
}

void QnSendmailBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    nx::vms::event::ActionParameters params;
    params.emailAddress = ui->emailLineEdit->text();
    model()->setActionParams(params);
}
