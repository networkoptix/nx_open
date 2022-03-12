// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "password_dialog.h"
#include "ui_password_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/branding.h>

namespace nx::vms::client::desktop {

PasswordDialog::PasswordDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::PasswordDialog())
{
    ui->setupUi(this);

    setWindowTitle(nx::branding::vmsName());

    ui->captionLabel->setStyleSheet(
        QString("QLabel { color: %1; }").arg(colorTheme()->color("light10").name()));
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

void PasswordDialog::setUsername(const QString& username, bool updateFocus)
{
    ui->usernameLineEdit->setText(username);

    if (!updateFocus)
        return;

    // Focus the password input when username is not empty.
    if (username.isEmpty())
        ui->usernameLineEdit->setFocus(Qt::OtherFocusReason);
    else
        ui->passwordLineEdit->setFocus(Qt::OtherFocusReason);
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
