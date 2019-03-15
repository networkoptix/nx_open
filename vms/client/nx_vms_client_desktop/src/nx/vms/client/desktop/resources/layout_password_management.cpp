#include "layout_password_management.h"

#include <QtCore/QCoreApplication>

#include <core/resource/layout_resource.h>
#include <core/resource/file_layout_resource.h>
#include <nx/core/layout/layout_file_info.h>
#include <core/resource_management/resource_runtime_data.h>

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
    const auto fileInfo = nx::core::layout::identifyFile(layout->getUrl());
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

bool isEncrypted(const QnLayoutResourcePtr& layout)
{
    const auto fileLayout = layout.dynamicCast<QnFileLayoutResource>();
    return fileLayout && fileLayout->isEncrypted();
}

QString password(const QnLayoutResourcePtr& layout)
{
    const auto fileLayout = layout.dynamicCast<QnFileLayoutResource>();
    return fileLayout ? fileLayout->password() : QString();
}

QString askPassword(const QnLayoutResourcePtr& layout, QWidget* parent)
{
    auto fileLayout = layout.dynamicCast<QnFileLayoutResource>();
    if (!NX_ASSERT(fileLayout))
        return QString();

    auto dialog = createPasswordDialog(
        EncryptedLayoutStrings::tr("The file %1 is encrypted. Please enter the password:").
            arg(QFileInfo(layout->getUrl()).fileName()),
        parent);

    dialog->setValidator(passwordValidator(layout));

    if (dialog->exec() == QDialog::Accepted)
    {
        fileLayout->usePasswordToRead(dialog->value().trimmed());
        return dialog->value().trimmed();
    }

    return QString();
}

bool confirmPassword(const QnLayoutResourcePtr& layout, QWidget* parent)
{
    auto dialog = createPasswordDialog(
        EncryptedLayoutStrings::tr("Please re-enter password for layout %1:").
            arg(QFileInfo(layout->getUrl()).fileName()),
        parent);
    dialog->setValidator(passwordValidator(layout));

    return dialog->exec() == QDialog::Accepted; //< The dialog checks the password itself.
}

} // namespace nx::vms::client::desktop::layout
