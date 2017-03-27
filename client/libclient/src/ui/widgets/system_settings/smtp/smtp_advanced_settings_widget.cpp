#include "smtp_advanced_settings_widget.h"
#include "ui_smtp_advanced_settings_widget.h"

#include <QtCore/QScopedValueRollback>

#include <ui/common/read_only.h>
#include <ui/common/aligner.h>
#include <ui/utils/validators.h>

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/app_info.h>
#include <utils/email/email.h>

namespace {
QList<QnEmail::ConnectionType> connectionTypesAllowed()
{
    return QList<QnEmail::ConnectionType>()
        << QnEmail::Unsecure
        << QnEmail::Ssl
        << QnEmail::Tls;
}


class QnPortNumberValidator : public QIntValidator
{
    typedef QIntValidator base_type;
public:
    QnPortNumberValidator(const QString &autoString, QObject* parent = 0) :
        base_type(parent), m_autoString(autoString)
    {}

    virtual QValidator::State validate(QString &input, int &pos) const override
    {
        if (m_autoString.compare(input, Qt::CaseInsensitive) == 0)
            return QValidator::Acceptable;

        if (m_autoString.startsWith(input, Qt::CaseInsensitive))
            return QValidator::Intermediate;

        QValidator::State result = base_type::validate(input, pos);
        if (result == QValidator::Acceptable && (input.toInt() == 0 || input.toInt() > 65535))
            return QValidator::Intermediate;
        return result;
    }

    virtual void fixup(QString &input) const override
    {
        if (m_autoString.compare(input, Qt::CaseInsensitive) == 0)
            return;
        if (input.toInt() == 0 || input.toInt() > USHRT_MAX)
            input = m_autoString;
    }
private:
    QString m_autoString;
};

}

QnSmtpAdvancedSettingsWidget::QnSmtpAdvancedSettingsWidget(QWidget* parent /*= nullptr*/) :
    QWidget(parent),
    ui(new Ui::SmtpAdvancedSettingsWidget),
    m_updating(false),
    m_readOnly(false)
{
    ui->setupUi(this);

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator(Qn::defaultEmailValidator());

    ui->serverInputField->setTitle(tr("SMTP Server"));
    ui->serverInputField->setValidator(Qn::defaultNonEmptyValidator(tr("Server cannot be empty.")));

    ui->userInputField->setTitle(tr("User"));
    ui->passwordInputField->setTitle(tr("Password"));
    ui->passwordInputField->setEchoMode(QLineEdit::Password);

    ui->signatureInputField->setTitle(tr("System Signature"));
    ui->signatureInputField->setPlaceholderText(tr("Enter a short System description here."));

    ui->supportInputField->setTitle(tr("Support Signature"));
    ui->supportInputField->setPlaceholderText(QnAppInfo::supportUrl());

    const QString autoPort = tr("Auto");
    ui->portComboBox->addItem(autoPort, 0);
    for (QnEmail::ConnectionType type : connectionTypesAllowed())
    {
        int port = QnEmailSettings::defaultPort(type);
        ui->portComboBox->addItem(QString::number(port), port);
    }
    ui->portComboBox->setValidator(new QnPortNumberValidator(autoPort, this));

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());

    for (auto field : {
        ui->emailInputField,
        ui->serverInputField,
        ui->userInputField,
        ui->passwordInputField,
        ui->signatureInputField,
        ui->supportInputField })
    {
        connect(field, &QnInputField::textChanged, this, &QnSmtpAdvancedSettingsWidget::settingsChanged);
        aligner->addWidget(field);
    }

    connect(ui->portComboBox, QnComboboxCurrentIndexChanged, this, &QnSmtpAdvancedSettingsWidget::settingsChanged);
    connect(ui->tlsRadioButton, &QRadioButton::toggled, this, &QnSmtpAdvancedSettingsWidget::settingsChanged);
    connect(ui->sslRadioButton, &QRadioButton::toggled, this, &QnSmtpAdvancedSettingsWidget::settingsChanged);
    connect(ui->unsecuredRadioButton, &QRadioButton::toggled, this, &QnSmtpAdvancedSettingsWidget::settingsChanged);

    connect(ui->portComboBox, QnComboboxCurrentIndexChanged, this, &QnSmtpAdvancedSettingsWidget::at_portComboBox_currentIndexChanged);
}

QnSmtpAdvancedSettingsWidget::~QnSmtpAdvancedSettingsWidget()
{}

QnEmailSettings QnSmtpAdvancedSettingsWidget::settings() const
{
    QnEmailSettings result;
    result.server = ui->serverInputField->text();
    result.email = ui->emailInputField->text();
    result.port = ui->portComboBox->currentText().toInt();
    result.user = ui->userInputField->text();
    result.password = ui->passwordInputField->text();
    result.connectionType = ui->tlsRadioButton->isChecked()
        ? QnEmail::Tls
        : ui->sslRadioButton->isChecked()
        ? QnEmail::Ssl
        : QnEmail::Unsecure;
    result.simple = false;
    result.signature = ui->signatureInputField->text();
    result.supportEmail = ui->supportInputField->text();
    return result;
}

void QnSmtpAdvancedSettingsWidget::setSettings(const QnEmailSettings &value)
{
    QScopedValueRollback<bool> guard(m_updating, true);
    setConnectionType(value.connectionType, value.port);
    ui->serverInputField->setText(value.server);
    ui->userInputField->setText(value.user);
    ui->emailInputField->setText(value.email);
    ui->passwordInputField->setText(value.password);
    ui->signatureInputField->setText(value.signature);
    ui->supportInputField->setText(value.supportEmail);
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

void QnSmtpAdvancedSettingsWidget::setConnectionType(QnEmail::ConnectionType connectionType, int port /*= 0*/)
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
        case QnEmail::Tls:
            ui->tlsRadioButton->setChecked(true);
            break;
        case QnEmail::Ssl:
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
    if (port == QnEmailSettings::defaultPort(QnEmail::Ssl))
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

