// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_credentials_dialog.h"
#include "ui_camera_credentials_dialog.h"

#include <nx/vms/client/desktop/common/utils/aligner.h>

namespace nx::vms::client::desktop {

CameraCredentialsDialog::CameraCredentialsDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraCredentialsDialog())
{
    ui->setupUi(this);
    setResizeToContentsMode(Qt::Vertical);

    ui->loginInputField->setTitle(tr("Login"));
    ui->passwordInputField->setTitle(tr("Password"));

    auto aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    aligner->addWidgets({ui->loginInputField, ui->passwordInputField});

    connect(ui->loginInputField->lineEdit(), &QLineEdit::textEdited, this,
        [passwordInput = ui->passwordInputField]
        {
            if (passwordInput->hasRemotePassword() || !passwordInput->optionalText())
                passwordInput->setOptionalText(QString());
        });
}

CameraCredentialsDialog::~CameraCredentialsDialog()
{
}

std::optional<QString> CameraCredentialsDialog::login() const
{
    return ui->loginInputField->optionalText();
}

void CameraCredentialsDialog::setLogin(const std::optional<QString>& value)
{
    ui->loginInputField->setOptionalText(value);
}

std::optional<QString> CameraCredentialsDialog::password() const
{
    return ui->passwordInputField->optionalText();
}

void CameraCredentialsDialog::setPassword(const std::optional<QString>& value)
{
    ui->passwordInputField->setOptionalText(value);
}

bool CameraCredentialsDialog::hasRemotePassword() const
{
    return ui->passwordInputField->hasRemotePassword();
}

void CameraCredentialsDialog::setHasRemotePassword(bool value)
{
    ui->passwordInputField->setHasRemotePassword(value);
}

} // namespace nx::vms::client::desktop
