// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "smtp_simple_settings_widget.h"
#include "ui_smtp_simple_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <ui/common/read_only.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/validators.h>

#include <utils/email/email.h>

#include <nx/branding.h>

using namespace nx::vms::client::desktop;

QnEmailSettings QnSimpleSmtpSettings::toSettings(const QnEmailSettings &base) const
{
    QnEmailSettings result;
    QnEmailAddress address(email);
    if (address.smtpServer().isNull())
    {
        /* Current simple settings are not valid. */
        result = base;
    }
    else
    {
        result = address.settings();
        result.user = address.value();
    }
    result.email = email;
    result.password = password;
    result.signature = signature;
    result.supportEmail = supportEmail;
    return result;
}

QnSimpleSmtpSettings QnSimpleSmtpSettings::fromSettings(const QnEmailSettings &source)
{
    QnSimpleSmtpSettings result;
    result.email = source.email;
    result.password = source.password;
    result.signature = source.signature;
    result.supportEmail = source.supportEmail;
    return result;
}

QnSmtpSimpleSettingsWidget::QnSmtpSimpleSettingsWidget(QWidget* parent /*= nullptr*/) :
    QWidget(parent),
    ui(new Ui::SmtpSimpleSettingsWidget),
    m_updating(false),
    m_readOnly(false)
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
    ui->passwordInputField->setValidator(defaultNonEmptyValidator(tr("Password cannot be empty.")));

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

QnSimpleSmtpSettings QnSmtpSimpleSettingsWidget::settings() const
{
    QnSimpleSmtpSettings result;
    result.email = ui->emailInputField->text();
    result.password = ui->passwordInputField->text();
    result.signature = ui->signatureInputField->text();
    result.supportEmail = ui->supportInputField->text();
    return result;
}

void QnSmtpSimpleSettingsWidget::setSettings(const QnSimpleSmtpSettings &value)
{
    QScopedValueRollback<bool> guard(m_updating, true);

    ui->emailInputField->setText(value.email);
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

void QnSmtpSimpleSettingsWidget::setHasRemotePassword(bool value)
{
    ui->passwordInputField->setHasRemotePassword(value);
}
