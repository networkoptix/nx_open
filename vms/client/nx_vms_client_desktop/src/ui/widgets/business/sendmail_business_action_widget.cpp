// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sendmail_business_action_widget.h"
#include "ui_sendmail_business_action_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>

#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/event/action_parameters.h>

using namespace nx::vms::client::desktop;

QnSendmailBusinessActionWidget::QnSendmailBusinessActionWidget(
    WindowContext* context,
    QWidget* parent)
    :
    base_type(context->system(), parent),
    ui(new Ui::SendmailBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->emailLineEdit, &QLineEdit::textChanged,
        this, &QnSendmailBusinessActionWidget::paramsChanged);

    connect(ui->settingsButton, &QPushButton::clicked,
        context->menu()->action(menu::PreferencesSmtpTabAction), &QAction::trigger);
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
