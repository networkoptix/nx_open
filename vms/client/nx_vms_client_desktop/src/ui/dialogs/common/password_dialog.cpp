#include "password_dialog.h"
#include "ui_password_dialog.h"

namespace nx::vms::client::desktop {

PasswordDialog::PasswordDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::PasswordDialog())
{
    ui->setupUi(this);

    ui->passwordLineEdit->useForPassword();

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

QString PasswordDialog::username() const
{
    return ui->usernameLineEdit->text();
}

QString PasswordDialog::password() const
{
    return ui->passwordLineEdit->text();
}

} // namespace nx::vms::client::desktop
