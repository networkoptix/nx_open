// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "smtp_simple_settings_widget.h"
#include "ui_smtp_simple_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <nx/branding.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <ui/common/read_only.h>

using namespace nx::vms::client::desktop;

QnSmtpSimpleSettingsWidget::QnSmtpSimpleSettingsWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::SmtpSimpleSettingsWidget)
{
    ui->setupUi(this);

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator([](const QString& text)
    {
        if (!nx::email::isValidAddress(text))
            return ValidationResult(tr("Email is not valid."));

        QnEmailAddress email(text);
        if (email.smtpServer().isNull())
            return ValidationResult(tr("No preset found. Use \"Advanced\" option."));

        return ValidationResult::kValid;
    });

    // TODO: #sivanov Strings duplication.
    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setValidator(defaultPasswordValidator(/*allowEmpty*/ false));

    // TODO: #sivanov Strings duplication.
    ui->signatureInputField->setTitle(tr("System Signature"));
    ui->signatureInputField->setPlaceholderText(tr("Enter a short System description here."));
    ui->supportInputField->setTitle(tr("Support Signature"));
    ui->supportInputField->setPlaceholderText(nx::branding::supportAddress());

    Aligner* aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());

    const QVector<InputField*> fields = {
        ui->emailInputField,
        ui->passwordInputField,
        ui->signatureInputField,
        ui->supportInputField};

    for (auto field: fields)
    {
        connect(field, &InputField::textChanged,
            this, &QnSmtpSimpleSettingsWidget::settingsChanged);

        aligner->addWidget(field);
    }
}

QnSmtpSimpleSettingsWidget::~QnSmtpSimpleSettingsWidget()
{
}

QnEmailSettings QnSmtpSimpleSettingsWidget::settings(const QnEmailSettings& baseSettings) const
{
    QnEmailSettings result;

    QnEmailAddress emailAddress(ui->emailInputField->text());
    if (!emailAddress.smtpServer().isNull())
    {
        result = emailAddress.settings();
        result.user = emailAddress.value();
    }
    else
    {
        result = baseSettings;
    }

    result.email = ui->emailInputField->text();
    result.password = ui->passwordInputField->text();
    result.signature = ui->signatureInputField->text();
    result.supportEmail = ui->supportInputField->text();
    return result;
}

void QnSmtpSimpleSettingsWidget::setSettings(const QnEmailSettings& value)
{
    QScopedValueRollback<bool> guard(m_updating, true);

    ui->emailInputField->setText(value.email);
    if (!ui->passwordInputField->hasRemotePassword() || !value.password.isEmpty())
        ui->passwordInputField->setText(value.password);
    ui->signatureInputField->setText(value.signature);
    ui->supportInputField->setText(value.supportEmail);
}

bool QnSmtpSimpleSettingsWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnSmtpSimpleSettingsWidget::setReadOnly(bool readOnly)
{
    using ::setReadOnly;

    setReadOnly(ui->emailInputField, readOnly);
    setReadOnly(ui->passwordInputField, readOnly);
    setReadOnly(ui->signatureInputField, readOnly);
    setReadOnly(ui->supportInputField, readOnly);
}

bool QnSmtpSimpleSettingsWidget::hasRemotePassword() const
{
    return ui->passwordInputField->hasRemotePassword();
}

void QnSmtpSimpleSettingsWidget::setHasRemotePassword(bool value)
{
    ui->passwordInputField->setHasRemotePassword(value);
}
