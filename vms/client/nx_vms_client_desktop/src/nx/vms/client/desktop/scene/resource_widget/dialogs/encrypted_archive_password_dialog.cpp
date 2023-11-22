// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "encrypted_archive_password_dialog.h"
#include "ui_encrypted_archive_password_dialog.h"

#include <vector>

#include <QtGui/QFont>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/crypt/crypt.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

EncryptedArchivePasswordDialog::EncryptedArchivePasswordDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::EncryptedArchivePasswordDialog())
{
    ui->setupUi(this);
    setModal(true);
    setResizeToContentsMode(Qt::Vertical);
    ui->salt->setVisible(false);

    QPushButton* decryptButton = ui->buttonBox->button(QDialogButtonBox::Apply);
    decryptButton->setText(tr("Decrypt"));
    decryptButton->setDefault(true);
    setAccentStyle(decryptButton);

    ui->iconLabel->setPixmap(
        core::Skin::maximumSizePixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning)));

    QFont font = ui->textLabel->font();
    font.setWeight(QFont::Medium);
    font.setPixelSize(fontConfig()->large().pixelSize());
    ui->textLabel->setFont(font);
    setPaletteColor(ui->textLabel, QPalette::WindowText, core::colorTheme()->color("light10"));

    connect(decryptButton, &QPushButton::clicked, this,
        [this]()
        {
            ui->inputField->validate();
            if (!ui->inputField->isValid())
                return;

            const auto callback = nx::utils::guarded(this,
                [this](bool, rest::Handle, const rest::ServerConnection::EmptyResponseType&)
                {
                    accept();
                });

            auto api = system()->connectedServerApi();
            if (NX_ASSERT(api))
            {
                api->setStorageEncryptionPassword(
                    ui->inputField->text(),
                    /*makeCurrent*/ false,
                    QByteArray::fromBase64(ui->salt->text().toUtf8()),
                    callback,
                    thread());
            }
        });
}

EncryptedArchivePasswordDialog::~EncryptedArchivePasswordDialog()
{
}

void EncryptedArchivePasswordDialog::showForEncryptionData(const QByteArray& encryptionData)
{
    ui->inputField->clear();
    std::vector<uint8_t> data(encryptionData.begin(), encryptionData.end());
    const auto [key, nonce] = nx::vms::crypt::fromEncryptionData(data);
    ui->salt->setText(key.salt.toBase64());

    ui->inputField->setValidator(
        [encryptionData](const QString& password) -> ValidationResult
        {
            ValidationResult result(QValidator::Acceptable);
            std::vector<uint8_t> data(encryptionData.begin(), encryptionData.end());
            if (nx::vms::crypt::makeKey(password, data).isEmpty())
            {
                result.state = QValidator::Invalid;
                result.errorMessage = tr("Invalid password");
            }
            return result;
        });

    show();
}

} // namespace nx::vms::client::desktop
