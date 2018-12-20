#include "layout_password_management.h"

#include <QtCore/QCoreApplication>

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_runtime_data.h>
#include <nx/core/layout/layout_file_info.h>

#include <client/client_globals.h>

#include <ui/dialogs/common/input_dialog.h>

namespace {

class EncryptedLayoutStrings
{
    Q_DECLARE_TR_FUNCTIONS(EncryptedLayoutStrings)
};


} // namespace

namespace nx::vms::client::desktop::layout {

namespace {

// Returns lambda to validate a password based on checksum.
auto passwordValidator(const QnLayoutResourcePtr& layout)
{
    const auto fileInfo = core::layout::identifyFile(layout->getUrl());
    return
        [fileInfo](const QString& value) -> ValidationResult
    {
        if (value.isEmpty())
            return ValidationResult(EncryptedLayoutStrings::tr("Please enter a valid password"));

        if (!checkPassword(value.trimmed(), fileInfo)) //< Trimming password!
            return ValidationResult(EncryptedLayoutStrings::tr("The password is not valid."));

        return ValidationResult(QValidator::Acceptable);
    };
};

QnInputDialog* createPasswordDialog(const QString message, QWidget* parent)
{
    auto dialog = new QnInputDialog(parent);
    dialog->setButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialog->setWindowTitle(EncryptedLayoutStrings::tr("Encrypted layout"));
    dialog->setCaption(message);
    dialog->useForPassword();
    return dialog;
}

} // namespace

void markAsEncrypted(const QnLayoutResourcePtr& layout, bool value)
{
    NX_ASSERT(layout->isFile());
    qnResourceRuntimeDataManager->setResourceData(layout, Qn::LayoutEncryptionRole, value);
}

bool isEncrypted(const QnLayoutResourcePtr& layout)
{
    return layout->isFile()
        && qnResourceRuntimeDataManager->resourceData(layout, Qn::LayoutEncryptionRole).toBool();
}

QString password(const QnLayoutResourcePtr& layout)
{
    return qnResourceRuntimeDataManager->resourceData(layout, Qn::LayoutPasswordRole).toString();
}

bool requiresPassword(const QnLayoutResourcePtr& layout)
{
    return isEncrypted(layout)
        && password(layout).isEmpty();
}

void usePasswordToOpen(const QnLayoutResourcePtr& layout, const QString& password)
{
    NX_ASSERT(isEncrypted(layout));
    qnResourceRuntimeDataManager->setResourceData(layout, Qn::LayoutPasswordRole, password);
    layout->usePasswordForRecordings(password);
}

void forgetPassword(const QnLayoutResourcePtr& layout)
{
    NX_ASSERT(isEncrypted(layout));
    qnResourceRuntimeDataManager->cleanupResourceData(layout, Qn::LayoutPasswordRole);
    layout->forgetPasswordForRecordings();
}

bool askAndSetPassword(const QnLayoutResourcePtr& layout, QWidget* parent)
{
    auto dialog = createPasswordDialog(
        EncryptedLayoutStrings::tr("This file is encrypted. Please enter the password:"),
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
    auto dialog = createPasswordDialog(
        EncryptedLayoutStrings::tr("Please re-enter password for this layout:"),
        parent);
    dialog->setValidator(passwordValidator(layout));

    return dialog->exec() == QDialog::Accepted; //< The dialog checks the password itself.
}

} // namespace nx::vms::client::desktop::layout
