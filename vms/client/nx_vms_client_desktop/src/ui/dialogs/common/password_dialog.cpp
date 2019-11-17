#include "password_dialog.h"
#include "ui_password_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/utils/app_info.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace nx::vms::client::desktop {

PasswordDialog::PasswordDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::PasswordDialog())
{
    ui->setupUi(this);

    setWindowTitle(nx::utils::AppInfo::vmsName());

    ui->captionLabel->setStyleSheet(
        QString("QLabel { color: %1; }").arg(colorTheme()->color("light10").name()));
    ui->passwordLineEdit->useForPassword();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Sign In"));

    setResizeToContentsMode(Qt::Horizontal | Qt::Vertical);
}

PasswordDialog::~PasswordDialog()
{
}

QString PasswordDialog::caption() const
{
    return ui->captionLabel->text();
}

void PasswordDialog::setCaption(const QString& caption)
{
    ui->captionLabel->setText(caption);
}

QString PasswordDialog::text() const
{
    return ui->textLabel->text();
}

void PasswordDialog::setText(const QString& text)
{
    ui->textLabel->setText(text);
}

QString PasswordDialog::username() const
{
    return ui->usernameLineEdit->text();
}

QString PasswordDialog::password() const
{
    return ui->passwordLineEdit->text();
}

} // namespace nx::vms::client::desktop
