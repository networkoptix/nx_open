// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "change_user_password_dialog.h"
#include "ui_change_user_password_dialog.h"

#include <vector>

#include <core/resource/user_resource.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/common/widgets/password_strength_indicator.h>

using namespace nx::vms::client::desktop;

QnChangeUserPasswordDialog::QnChangeUserPasswordDialog(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, QnWorkbenchContextAware::InitializationMode::lazy),
    ui(new Ui::ChangeUserPasswordDialog())
{
    ui->setupUi(this);

    ui->newPasswordInputField->setTitle(tr("New Password"));
    ui->newPasswordInputField->setPasswordIndicatorEnabled(true);
    ui->newPasswordInputField->setValidator(defaultPasswordValidator(false));
    ui->newPasswordInputField->reset();

    ui->confirmPasswordInputField->setTitle(tr("Confirm Password"));
    ui->confirmPasswordInputField->setValidator(defaultConfirmationValidator(
        [this]() { return ui->newPasswordInputField->text(); },
        tr("Passwords do not match")));

    ui->currentPasswordInputField->setTitle(tr("Current Password"));
    ui->currentPasswordInputField->setValidator(
        [this](const QString& text)
        {
            if (!context()->user())
                return ValidationResult::kValid;

            if (text.isEmpty())
                return ValidationResult(
                    tr("To modify your password please enter the existing one"));

            if (!context()->user()->getHash().checkPassword(text))
                return ValidationResult(tr("Invalid current password"));

            return ValidationResult::kValid;
        });

    Aligner* aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());

    aligner->addWidgets({
        ui->newPasswordInputField,
        ui->confirmPasswordInputField,
        ui->currentPasswordInputField
    });

    setInfoText({});

    setResizeToContentsMode(Qt::Vertical);
}

QnChangeUserPasswordDialog::~QnChangeUserPasswordDialog()
{
}

QString QnChangeUserPasswordDialog::newPassword() const
{
    std::vector<InputField*> fields = {
        ui->newPasswordInputField,
        ui->confirmPasswordInputField,
        ui->currentPasswordInputField
    };

    for (InputField* field: fields)
    {
        if (!field->isValid())
            return QString();
    }

    return ui->newPasswordInputField->text();
}

void QnChangeUserPasswordDialog::setInfoText(const QString& text)
{
    ui->infoBar->setText(text);
}

bool QnChangeUserPasswordDialog::validate()
{
    bool result = true;

    std::vector<InputField*> fields = {
        ui->newPasswordInputField,
        ui->confirmPasswordInputField,
        ui->currentPasswordInputField
    };

    for (InputField* field: fields)
        result = field->validate() && result;

    return result;
}

void QnChangeUserPasswordDialog::accept()
{
    if (!validate())
        return;

    base_type::accept();
}
