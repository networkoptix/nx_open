#include "workbench_layout_password_management.h"

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

//nx::client::desktop::TextValidateFunction

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {
namespace layout {

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

bool needPassword(const QnLayoutResourcePtr& layout)
{
    return layout->data(Qn::LayoutEncryptionRole).toBool()
        && layout->data(Qn::LayoutPasswordRole).toString().isEmpty();
}

bool askAndSetPassword(const QnLayoutResourcePtr& layout, QWidget* parent)
{
    auto dialog = createPasswordDialog(tr("This file is encrypted. Please enter the password:"),
        parent);
    dialog->setValidator(passwordValidator(layout));

    const bool res = dialog->exec() == QDialog::Accepted;

    const auto password = dialog->value().trimmed();

    if (res)
    {
        layout->setData(Qn::LayoutPasswordRole, password);
        layout->usePasswordToOpen(password);
    }

    return res;
}

bool confirmPassword(const QnLayoutResourcePtr& layout, QWidget* parent)
{
    auto dialog = createPasswordDialog(tr("Please re-enter password for this layout:"), parent);
    dialog->setValidator(passwordValidator(layout));

    return dialog->exec() == QDialog::Accepted; //< The dialog checks the password itself.
}

} // namespace layout
} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx