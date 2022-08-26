// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "smtp_advanced_settings_widget.h"
#include "ui_smtp_advanced_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <nx/branding.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/email/email.h>

using namespace nx::vms::client::desktop;

namespace {

QList<QnEmail::ConnectionType> connectionTypesAllowed()
{
    return QList<QnEmail::ConnectionType>()
        << QnEmail::ConnectionType::unsecure
        << QnEmail::ConnectionType::ssl
        << QnEmail::ConnectionType::tls;
} // namespace

class QnPortNumberValidator: public QIntValidator
{
    using base_type = QIntValidator;

public:
    QnPortNumberValidator(const QString& autoString, QObject* parent = nullptr):
        base_type(parent),
        m_autoString(autoString)
    {}

    virtual QValidator::State validate(QString& input, int& pos) const override
    {
        if (m_autoString.compare(input, Qt::CaseInsensitive) == 0)
            return QValidator::Acceptable;

        if (m_autoString.startsWith(input, Qt::CaseInsensitive))
            return QValidator::Intermediate;

        QValidator::State result = base_type::validate(input, pos);
        if (result == QValidator::Acceptable && (input.toInt() > 0 && input.toInt() < 65536))
            return QValidator::Intermediate;
        else
            return QValidator::Invalid;
    };

    virtual void fixup(QString& input) const override
    {
        if (m_autoString.compare(input, Qt::CaseInsensitive) == 0)
            return;

        if (input.toInt() == 0 || input.toInt() > USHRT_MAX)
            input = m_autoString;
    };

private:
    QString m_autoString;
};

} // namespace

QnSmtpAdvancedSettingsWidget::QnSmtpAdvancedSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::SmtpAdvancedSettingsWidget)
{
    ui->setupUi(this);

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator(defaultEmailValidator());

    ui->serverInputField->setTitle(tr("SMTP Server"));
    ui->serverInputField->setValidator(defaultNonEmptyValidator(tr("Server cannot be empty.")));

    ui->userInputField->setTitle(tr("User"));

    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setValidator(defaultPasswordValidator(/*allowEmpty*/ true));

    ui->signatureInputField->setTitle(tr("System Signature"));
    ui->signatureInputField->setPlaceholderText(tr("Enter a short System description here."));

    ui->supportInputField->setTitle(tr("Support Signature"));
    ui->supportInputField->setPlaceholderText(nx::branding::supportAddress());

    const QString autoPort = tr("Auto");
    ui->portComboBox->addItem(autoPort, 0);

    for (QnEmail::ConnectionType type: connectionTypesAllowed())
    {
        int port = QnEmailSettings::defaultPort(type);
        ui->portComboBox->addItem(QString::number(port), port);
    }

    ui->portComboBox->setValidator(new QnPortNumberValidator(autoPort, this));

    Aligner* aligner = new Aligner(this);
    aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());

    const QVector<InputField*> fields = {
        ui->emailInputField,
        ui->serverInputField,
        ui->userInputField,
        ui->passwordInputField,
        ui->signatureInputField,
        ui->supportInputField};

    for (auto field: fields)
    {
        connect(field, &InputField::textChanged,
            this, &QnSmtpAdvancedSettingsWidget::settingsChanged);

        aligner->addWidget(field);
    }
    aligner->addWidget(ui->tlsRadioButtonAligner);
    aligner->addWidget(ui->sslRadioButtonAligner);
    aligner->addWidget(ui->unsecuredRadioButtonAligner);

    connect(ui->portComboBox, QnComboboxCurrentIndexChanged,
        this, &QnSmtpAdvancedSettingsWidget::settingsChanged);

    connect(ui->tlsRadioButton, &QRadioButton::toggled,
        this, &QnSmtpAdvancedSettingsWidget::settingsChanged);

    connect(ui->sslRadioButton, &QRadioButton::toggled,
        this, &QnSmtpAdvancedSettingsWidget::settingsChanged);

    connect(ui->unsecuredRadioButton, &QRadioButton::toggled,
        this, &QnSmtpAdvancedSettingsWidget::settingsChanged);

    connect(ui->portComboBox, QnComboboxCurrentIndexChanged,
        this, &QnSmtpAdvancedSettingsWidget::at_portComboBox_currentIndexChanged);
}

QnSmtpAdvancedSettingsWidget::~QnSmtpAdvancedSettingsWidget()
{
}

QnEmailSettings QnSmtpAdvancedSettingsWidget::settings() const
{
    QnEmailSettings result;
    result.server = ui->serverInputField->text();
    result.email = ui->emailInputField->text();
    result.port = ui->portComboBox->currentText().toInt();
    result.user = ui->userInputField->text();
    result.password = ui->passwordInputField->text();
    result.connectionType = ui->tlsRadioButton->isChecked()
        ? QnEmail::ConnectionType::tls
        : ui->sslRadioButton->isChecked()
        ? QnEmail::ConnectionType::ssl
        : QnEmail::ConnectionType::unsecure;
    result.simple = false;
    result.signature = ui->signatureInputField->text();
    result.supportEmail = ui->supportInputField->text();
    return result;
}

void QnSmtpAdvancedSettingsWidget::setSettings(const QnEmailSettings& value)
{
    QScopedValueRollback<bool> guard(m_updating, true);
    setConnectionType(value.connectionType, value.port);
    ui->serverInputField->setText(value.server);
    ui->userInputField->setText(value.user);
    ui->emailInputField->setText(value.email);
    if (!ui->passwordInputField->hasRemotePassword() || !value.password.isNull())
        ui->passwordInputField->setText(value.password);
    ui->signatureInputField->setText(value.signature);
    ui->supportInputField->setText(value.supportEmail);
}

bool QnSmtpAdvancedSettingsWidget::hasRemotePassword() const
{
    return ui->passwordInputField->hasRemotePassword();
}

void QnSmtpAdvancedSettingsWidget::setHasRemotePassword(bool value)
{
    ui->passwordInputField->setHasRemotePassword(value);
}

bool QnSmtpAdvancedSettingsWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnSmtpAdvancedSettingsWidget::setReadOnly(bool readOnly)
{
    using ::setReadOnly;

    setReadOnly(ui->serverInputField, readOnly);
    setReadOnly(ui->emailInputField, readOnly);
    setReadOnly(ui->portComboBox, readOnly);
    setReadOnly(ui->userInputField, readOnly);
    setReadOnly(ui->passwordInputField, readOnly);
    setReadOnly(ui->tlsRadioButton, readOnly);
    setReadOnly(ui->sslRadioButton, readOnly);
    setReadOnly(ui->unsecuredRadioButton, readOnly);
    setReadOnly(ui->signatureInputField, readOnly);
    setReadOnly(ui->supportInputField, readOnly);
}

void QnSmtpAdvancedSettingsWidget::setConnectionType(
    QnEmail::ConnectionType connectionType,
    int port)
{
    bool portFound = false;
    for (int i = 0; i < ui->portComboBox->count(); i++)
    {
        if (ui->portComboBox->itemData(i).toInt() == port)
        {
            ui->portComboBox->setCurrentIndex(i);
            portFound = true;
            break;
        }
    }
    if (!portFound)
    {
        ui->portComboBox->setEditText(QString::number(port));
        ui->tlsRecommendedLabel->show();
        ui->sslRecommendedLabel->hide();
    }

    switch (connectionType)
    {
        case QnEmail::ConnectionType::tls:
            ui->tlsRadioButton->setChecked(true);
            break;
        case QnEmail::ConnectionType::ssl:
            ui->sslRadioButton->setChecked(true);
            break;
        default:
            ui->unsecuredRadioButton->setChecked(true);
            break;
    }
}

void QnSmtpAdvancedSettingsWidget::at_portComboBox_currentIndexChanged(int index)
{
    if (m_updating)
        return;

    int port = ui->portComboBox->itemData(index).toInt();
    if (port == QnEmailSettings::defaultPort(QnEmail::ConnectionType::ssl))
    {
        ui->tlsRecommendedLabel->hide();
        ui->sslRecommendedLabel->show();
    }
    else
    {
        ui->tlsRecommendedLabel->show();
        ui->sslRecommendedLabel->hide();
    }
}

