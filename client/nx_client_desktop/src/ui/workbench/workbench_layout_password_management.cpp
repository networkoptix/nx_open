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

bool needPassword(const QnLayoutResourcePtr& layout)
{
    return layout->data(Qn::LayoutEncryptionRole).toBool()
        && layout->data(Qn::LayoutPasswordRole).toString().isEmpty();
}

bool askAndSetPassword(const QnLayoutResourcePtr& layout, QWidget* parent)
{
    QnInputDialog* dialog = new QnInputDialog(parent);
    dialog->setButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialog->setWindowTitle(tr("Encrypted layout"));
    dialog->setCaption(tr("Please enter the password to open this layout:"));
    dialog->useForPassword();

    const auto fileInfo = core::layout::identifyFile(layout->getUrl());
    dialog->setValidator(
        [fileInfo](const QString& value) -> ValidationResult
        {
            if (value.isEmpty())
                return ValidationResult(tr("Please enter a valid password"));

            if (!checkPassword(value.trimmed(), fileInfo)) //< Trimming password!
                return ValidationResult(tr("The password is not valid."));

            return ValidationResult(QValidator::Acceptable);
        });

    bool res = dialog->exec() == QDialog::Accepted;

    const auto password = dialog->value().trimmed();

    if (res)
    {
        layout->setData(Qn::LayoutPasswordRole, password);
        layout->usePasswordToOpen(password);
    }

    return res;
}

} // namespace layout
} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx