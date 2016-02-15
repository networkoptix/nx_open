#include "smtp_advanced_settings_widget.h"
#include "ui_smtp_advanced_settings_widget.h"

#include <ui/common/mandatory.h>
#include <ui/common/read_only.h>
#include <ui/style/custom_style.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/app_info.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/email/email.h>

namespace {
    QList<QnEmail::ConnectionType> connectionTypesAllowed() {
        return QList<QnEmail::ConnectionType>() 
            << QnEmail::Unsecure
            << QnEmail::Ssl
            << QnEmail::Tls;
    }


    class QnPortNumberValidator: public QIntValidator {
        typedef QIntValidator base_type;
    public:
        QnPortNumberValidator(const QString &autoString, QObject* parent = 0):
            base_type(parent), m_autoString(autoString) {}

        virtual QValidator::State validate(QString &input, int &pos) const override {
            if (m_autoString.compare(input, Qt::CaseInsensitive) == 0)
                return QValidator::Acceptable;

            if (m_autoString.startsWith(input, Qt::CaseInsensitive))
                return QValidator::Intermediate;

            QValidator::State result = base_type::validate(input, pos);
            if (result == QValidator::Acceptable && (input.toInt() == 0 || input.toInt() > 65535))
                return QValidator::Intermediate;
            return result;
        }

        virtual void fixup(QString &input) const override {
            if (m_autoString.compare(input, Qt::CaseInsensitive) == 0)
                return;
            if (input.toInt() == 0 || input.toInt() > USHRT_MAX)
                input = m_autoString;
        }
    private:
        QString m_autoString;
    };

}

QnSmtpAdvancedSettingsWidget::QnSmtpAdvancedSettingsWidget( QWidget* parent /*= nullptr*/ )
    : QWidget(parent)
    , ui(new Ui::SmtpAdvancedSettingsWidget)
    , m_updating(false)
    , m_readOnly(false)
{
    ui->setupUi(this);

    setWarningStyle(ui->supportEmailWarningLabel);

    const QString autoPort = tr("Auto");
    ui->portComboBox->addItem(autoPort, 0);
    for (QnEmail::ConnectionType type: connectionTypesAllowed()) {
        int port = QnEmailSettings::defaultPort(type);
        ui->portComboBox->addItem(QString::number(port), port);
    }
    ui->portComboBox->setValidator(new QnPortNumberValidator(autoPort, this));

    ui->supportLinkLineEdit->setPlaceholderText(QnAppInfo::supportLink());


    declareMandatoryField(ui->emailLabel);
    declareMandatoryField(ui->serverLabel);
    declareMandatoryField(ui->userLabel);
    declareMandatoryField(ui->passwordLabel);

    auto listenTo = [this](QLineEdit *lineEdit) {
        connect(lineEdit, &QLineEdit::textChanged, this, &QnSmtpAdvancedSettingsWidget::settingsChanged);
    };

    listenTo(ui->serverLineEdit);
    listenTo(ui->emailLineEdit);
    listenTo(ui->userLineEdit);
    listenTo(ui->passwordLineEdit);
    listenTo(ui->signatureLineEdit);
    listenTo(ui->supportLinkLineEdit);

    connect(ui->portComboBox,           QnComboboxCurrentIndexChanged,   this, &QnSmtpAdvancedSettingsWidget::settingsChanged);
    connect(ui->tlsRadioButton,         &QRadioButton::toggled,   this, &QnSmtpAdvancedSettingsWidget::settingsChanged);
    connect(ui->sslRadioButton,         &QRadioButton::toggled,   this, &QnSmtpAdvancedSettingsWidget::settingsChanged);
    connect(ui->unsecuredRadioButton,   &QRadioButton::toggled,   this, &QnSmtpAdvancedSettingsWidget::settingsChanged);

    connect(ui->portComboBox,               QnComboboxCurrentIndexChanged,      this,   &QnSmtpAdvancedSettingsWidget::at_portComboBox_currentIndexChanged);
    connect(ui->emailLineEdit,              &QLineEdit::textChanged,            this,   &QnSmtpAdvancedSettingsWidget::validateEmail);
}

QnSmtpAdvancedSettingsWidget::~QnSmtpAdvancedSettingsWidget()
{}

QnEmailSettings QnSmtpAdvancedSettingsWidget::settings() const {
    QnEmailSettings result;
    result.server = ui->serverLineEdit->text();
    result.email = ui->emailLineEdit->text();
    result.port = ui->portComboBox->currentText().toInt();
    result.user = ui->userLineEdit->text();
    result.password = ui->passwordLineEdit->text();
    result.connectionType = ui->tlsRadioButton->isChecked()
        ? QnEmail::Tls
        : ui->sslRadioButton->isChecked()
        ? QnEmail::Ssl
        : QnEmail::Unsecure;
    result.simple = false;
    result.signature = ui->signatureLineEdit->text();
    result.supportEmail = ui->supportLinkLineEdit->text();
    return result;
}

void QnSmtpAdvancedSettingsWidget::setSettings( const QnEmailSettings &value ) {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    loadSettings(value.server, value.connectionType, value.port);
    ui->userLineEdit->setText(value.user);
    ui->emailLineEdit->setText(value.email);
    ui->passwordLineEdit->setText(value.password);
    ui->signatureLineEdit->setText(value.signature);
    ui->supportLinkLineEdit->setText(value.supportEmail);
}

bool QnSmtpAdvancedSettingsWidget::isReadOnly() const {
    return m_readOnly;
}

void QnSmtpAdvancedSettingsWidget::setReadOnly( bool readOnly ) {
    using ::setReadOnly;

    setReadOnly(ui->serverLineEdit, readOnly);
    setReadOnly(ui->emailLineEdit, readOnly);
    setReadOnly(ui->portComboBox, readOnly);
    setReadOnly(ui->userLineEdit, readOnly);
    setReadOnly(ui->passwordLineEdit, readOnly);
    setReadOnly(ui->tlsRadioButton, readOnly);
    setReadOnly(ui->sslRadioButton, readOnly);
    setReadOnly(ui->unsecuredRadioButton, readOnly);
    setReadOnly(ui->signatureLineEdit, readOnly);
    setReadOnly(ui->supportLinkLineEdit, readOnly);
}

void QnSmtpAdvancedSettingsWidget::loadSettings( const QString &server, QnEmail::ConnectionType connectionType, int port /*= 0*/ ) {
    ui->serverLineEdit->setText(server);

    bool portFound = false;
    for (int i = 0; i < ui->portComboBox->count(); i++) {
        if (ui->portComboBox->itemData(i).toInt() == port) {
            ui->portComboBox->setCurrentIndex(i);
            portFound = true;
            break;
        }
    }
    if (!portFound) {
        ui->portComboBox->setEditText(QString::number(port));
        ui->tlsRecommendedLabel->show();
        ui->sslRecommendedLabel->hide();
    }

    switch(connectionType) {
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


void QnSmtpAdvancedSettingsWidget::validateEmail() {
    QString errorText;

    const QString targetEmail = ui->emailLineEdit->text();

    if (!targetEmail.isEmpty()) {
        QnEmailAddress email(targetEmail); 
        if (!email.isValid())
            errorText = tr("E-Mail is not valid");
    }

    ui->supportEmailWarningLabel->setText(errorText);
}

void QnSmtpAdvancedSettingsWidget::at_portComboBox_currentIndexChanged( int index ) {
    if (m_updating)
        return;

    int port = ui->portComboBox->itemData(index).toInt();
    if (port == QnEmailSettings::defaultPort(QnEmail::Ssl)) {
        ui->tlsRecommendedLabel->hide();
        ui->sslRecommendedLabel->show();
    } else {
        ui->tlsRecommendedLabel->show();
        ui->sslRecommendedLabel->hide();
    }
}

