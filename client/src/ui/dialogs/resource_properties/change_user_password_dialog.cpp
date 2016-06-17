#include "change_user_password_dialog.h"
#include "ui_change_user_password_dialog.h"

#include <core/resource/user_resource.h>

#include <ui/common/aligner.h>
#include <ui/widgets/common/password_strength_indicator.h>
#include <ui/workbench/workbench_context.h>


QnChangeUserPasswordDialog::QnChangeUserPasswordDialog(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::ChangeUserPasswordDialog())
{
    ui->setupUi(this);

    ui->newPasswordInputField->setTitle(tr("New Password"));
    ui->newPasswordInputField->setPasswordMode(QLineEdit::Password, false, true);
    ui->newPasswordInputField->reset();

    ui->confirmPasswordInputField->setTitle(tr("Confirm Password"));
    ui->confirmPasswordInputField->setEchoMode(QLineEdit::Password);
    ui->confirmPasswordInputField->setConfirmationMode(ui->newPasswordInputField, tr("Passwords do not match."));

    ui->currentPasswordInputField->setTitle(tr("Current Password"));
    ui->currentPasswordInputField->setEchoMode(QLineEdit::Password);
    ui->currentPasswordInputField->setValidator([this](const QString& text)
    {
        if (text.isEmpty())
            return Qn::ValidationResult(tr("To modify your password, please enter the existing one."));

        if (!context()->user()->checkPassword(text))
            return Qn::ValidationResult(tr("Invalid current password."));

        return Qn::kValidResult;
    });

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());

    aligner->addWidgets({
        ui->newPasswordInputField,
        ui->confirmPasswordInputField,
        ui->currentPasswordInputField
    });
}

QnChangeUserPasswordDialog::~QnChangeUserPasswordDialog()
{
}

QString QnChangeUserPasswordDialog::newPassword() const
{
    for (QnInputField* field : { ui->newPasswordInputField, ui->confirmPasswordInputField, ui->currentPasswordInputField })
    {
        if (!field->isValid())
            return QString();
    }

    return ui->newPasswordInputField->text();
}

bool QnChangeUserPasswordDialog::validate()
{
    bool result = true;

    for (QnInputField* field : { ui->newPasswordInputField, ui->confirmPasswordInputField, ui->currentPasswordInputField })
        result = field->validate() && result;

    return result;
}

void QnChangeUserPasswordDialog::done(int r)
{
    if (r == Accepted && !validate())
        return;

    base_type::done(r);
}
