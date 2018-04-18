#include "change_user_password_dialog.h"
#include "ui_change_user_password_dialog.h"

#include <QtWidgets/QLineEdit>

#include <core/resource/user_resource.h>
#include <nx/client/desktop/common/utils/aligner.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/common/widgets/password_strength_indicator.h>

using namespace nx::client::desktop;

QnChangeUserPasswordDialog::QnChangeUserPasswordDialog(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, QnWorkbenchContextAware::InitializationMode::manual),
    ui(new Ui::ChangeUserPasswordDialog())
{
    ui->setupUi(this);

    ui->newPasswordInputField->setTitle(tr("New Password"));
    ui->newPasswordInputField->setEchoMode(QLineEdit::Password);
    ui->newPasswordInputField->setPasswordIndicatorEnabled(true);
    ui->newPasswordInputField->setValidator(defaultPasswordValidator(false));
    ui->newPasswordInputField->reset();

    ui->confirmPasswordInputField->setTitle(tr("Confirm Password"));
    ui->confirmPasswordInputField->setEchoMode(QLineEdit::Password);
    ui->confirmPasswordInputField->setValidator(defaultConfirmationValidator(
        [this]() { return ui->newPasswordInputField->text(); },
        tr("Passwords do not match.")));

    ui->currentPasswordInputField->setTitle(tr("Current Password"));
    ui->currentPasswordInputField->setEchoMode(QLineEdit::Password);
    ui->currentPasswordInputField->setValidator([this](const QString& text)
    {
        if (text.isEmpty())
            return ValidationResult(tr("To modify your password please enter the existing one."));

        if (!context()->user()->checkLocalUserPassword(text))
            return ValidationResult(tr("Invalid current password."));

        return ValidationResult::kValid;
    });

    Aligner* aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());

    aligner->addWidgets({
        ui->newPasswordInputField,
        ui->confirmPasswordInputField,
        ui->currentPasswordInputField
    });

    setResizeToContentsMode(Qt::Vertical);
}

QnChangeUserPasswordDialog::~QnChangeUserPasswordDialog()
{
}

QString QnChangeUserPasswordDialog::newPassword() const
{
    for (InputField* field : { ui->newPasswordInputField, ui->confirmPasswordInputField, ui->currentPasswordInputField })
    {
        if (!field->isValid())
            return QString();
    }

    return ui->newPasswordInputField->text();
}

bool QnChangeUserPasswordDialog::validate()
{
    bool result = true;

    for (InputField* field : { ui->newPasswordInputField, ui->confirmPasswordInputField, ui->currentPasswordInputField })
        result = field->validate() && result;

    return result;
}

void QnChangeUserPasswordDialog::accept()
{
    if (!validate())
        return;

    base_type::accept();
}
