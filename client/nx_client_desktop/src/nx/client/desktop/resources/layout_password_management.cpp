#include "layout_password_management.h"

#include <QtCore/QCoreApplication>

#include <core/resource/layout_resource.h>
#include <nx/core/layout/layout_file_info.h>

#include <client/client_globals.h>

#include <ui/dialogs/common/input_dialog.h>

namespace {

QString tr(const char* sourceText, const char* disambiguation = 0)
{
    return QCoreApplication::translate("EncryptedLayouts", sourceText, disambiguation);
}

} // namespace

namespace nx::client::desktop::layout {

namespace {

// Returns lambda to validate a password based on checksum.
auto passwordValidator(const QnLayoutResourcePtr& layout)
{
    const auto fileInfo = core::layout::identifyFile(layout->getUrl());
    return
        [fileInfo](const QString& value) -> ValidationResult
    {
        if (value.isEmpty())
            return ValidationResult(tr("Please enter a valid password"));

        if (!checkPassword(value.trimmed(), fileInfo)) //< Trimming password!
            return ValidationResult(tr("The password is not valid."));

        return ValidationResult(QValidator::Acceptable);
    };
};

QnInputDialog* createPasswordDialog(const QString message, QWidget* parent)
{
    auto dialog = new QnInputDialog(parent);
    dialog->setButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialog->setWindowTitle(tr("Encrypted layout"));
    dialog->setCaption(message);
    dialog->useForPassword();
    return dialog;
}

} // namespace

bool isEncrypted(const QnLayoutResourcePtr& layout)
{
    return layout->data(Qn::LayoutEncryptionRole).toBool();
}

bool requiresPassword(const QnLayoutResourcePtr& layout)
{
    return layout->data(Qn::LayoutEncryptionRole).toBool()
        && layout->data(Qn::LayoutPasswordRole).toString().isEmpty();
}

void usePasswordToOpen(const QnLayoutResourcePtr& layout, const QString& password)
{
    NX_ASSERT(layout->data(Qn::LayoutEncryptionRole).toBool());
    layout->setData(Qn::LayoutPasswordRole, password);
    layout->usePasswordForRecordings(password);
}

void forgetPassword(const QnLayoutResourcePtr& layout)
{
    NX_ASSERT(layout->data(Qn::LayoutEncryptionRole).toBool());
    layout->setData(Qn::LayoutPasswordRole, QVariant());
    layout->forgetPasswordForRecordings();
}

bool askAndSetPassword(const QnLayoutResourcePtr& layout, QWidget* parent)
{
    auto dialog = createPasswordDialog(tr("This file is encrypted. Please enter the password:"),
        parent);
    dialog->setValidator(passwordValidator(layout));

    if (dialog->exec() == QDialog::Accepted)
    {
        usePasswordToOpen(layout, dialog->value().trimmed());
        return true;
    }

    return false;
}

bool confirmPassword(const QnLayoutResourcePtr& layout, QWidget* parent)
{
    auto dialog = createPasswordDialog(tr("Please re-enter password for this layout:"), parent);
    dialog->setValidator(passwordValidator(layout));

    return dialog->exec() == QDialog::Accepted; //< The dialog checks the password itself.
}

} // namespace nx::client::desktop::layout
