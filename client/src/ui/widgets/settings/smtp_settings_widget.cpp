#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <nx_ec/ec_api.h>

#include <ui/style/warning_style.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include "version.h"

namespace {
    enum WidgetPages {
        SimplePage,
        AdvancedPage,
        TestingPage
    };

    const int testSmtpTimeoutMSec = 20 * 1000;
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

QnSmtpSettingsWidget::QnSmtpSettingsWidget(QWidget *parent) :
    QnAbstractPreferencesWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SmtpSettingsWidget),
    m_testHandle(-1),
    m_timeoutTimer(new QTimer(this))
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Server_Mail_Help);

    connect(ui->portComboBox,           SIGNAL(currentIndexChanged(int)),   this,   SLOT(at_portComboBox_currentIndexChanged(int)));
    connect(ui->advancedCheckBox,       SIGNAL(toggled(bool)),              this,   SLOT(at_advancedCheckBox_toggled(bool)));
    connect(ui->testButton,             SIGNAL(clicked()),                  this,   SLOT(at_testButton_clicked()));
    connect(ui->cancelTestButton,       SIGNAL(clicked()),                  this,   SLOT(at_cancelTestButton_clicked()));
    connect(ui->okTestButton,           SIGNAL(clicked()),                  this,   SLOT(at_okTestButton_clicked()));
    connect(m_timeoutTimer,             SIGNAL(timeout()),                  this,   SLOT(at_timer_timeout()));
    connect(ui->simpleEmailLineEdit,    &QLineEdit::textChanged,            this,   &QnSmtpSettingsWidget::validateEmailSimple);
    connect(ui->simpleSupportEmailLineEdit, &QLineEdit::textChanged,        this,   &QnSmtpSettingsWidget::validateEmailSimple);
    connect(ui->supportEmailLineEdit,   &QLineEdit::textChanged,            this,   &QnSmtpSettingsWidget::validateEmailAdvanced);

    m_timeoutTimer->setSingleShot(false);

    setWarningStyle(ui->detectErrorLabel);
    setWarningStyle(ui->supportEmailWarningLabel);

    const QString autoPort = tr("Auto");
    ui->portComboBox->addItem(autoPort, 0);
    for (int i = 0; i < QnEmail::ConnectionTypeCount; i++) {
        int port = QnEmail::defaultPort(static_cast<QnEmail::ConnectionType>(i));
        ui->portComboBox->addItem(QString::number(port), port);
    }
    ui->portComboBox->setValidator(new QnPortNumberValidator(autoPort, this));

    ui->supportEmailLineEdit->setPlaceholderText(lit(QN_SUPPORT_MAIL_ADDRESS));
    ui->simpleSupportEmailLineEdit->setPlaceholderText(lit(QN_SUPPORT_MAIL_ADDRESS));
}

QnSmtpSettingsWidget::~QnSmtpSettingsWidget()
{
}

void QnSmtpSettingsWidget::updateFromSettings() {
    QnEmail::Settings settings = QnGlobalSettings::instance()->emailSettings();

    loadSettings(settings.server, settings.connectionType, settings.port);
    ui->userLineEdit->setText(settings.user);
    ui->simpleEmailLineEdit->setText(settings.user);
    ui->passwordLineEdit->setText(settings.password);
    ui->simplePasswordLineEdit->setText(settings.password);
    ui->signatureLineEdit->setText(settings.signature);
    ui->simpleSignatureLineEdit->setText(settings.signature);
    ui->supportEmailLineEdit->setText(settings.supportEmail);
    ui->simpleSupportEmailLineEdit->setText(settings.supportEmail);
    ui->advancedCheckBox->setChecked(!settings.simple);
    ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
        ? AdvancedPage
        : SimplePage);
    updateFocusedElement();
}

void QnSmtpSettingsWidget::submitToSettings() {
    QnGlobalSettings::instance()->setEmailSettings(settings());
}

void QnSmtpSettingsWidget::updateFocusedElement() {
    switch (ui->stackedWidget->currentIndex()) {
    case SimplePage:
        ui->simpleEmailLineEdit->setFocus();
        break;
    case AdvancedPage:
        ui->serverLineEdit->setFocus();
        break;
    default:
        break;
    }
}

QnEmail::Settings QnSmtpSettingsWidget::settings() {

    QnEmail::Settings result;

    if (!ui->advancedCheckBox->isChecked()) {
        QnEmail email(ui->simpleEmailLineEdit->text());
        result = email.settings();
        result.user = ui->simpleEmailLineEdit->text();
        result.password = ui->simplePasswordLineEdit->text();
        result.simple = true;
        result.signature = ui->simpleSignatureLineEdit->text();
        result.supportEmail = ui->simpleSupportEmailLineEdit->text();
        return result;
    }

    result.server = ui->serverLineEdit->text();
    result.port = ui->portComboBox->currentText().toInt();
    result.user = ui->userLineEdit->text();
    result.password = ui->passwordLineEdit->text();
    result.connectionType = ui->tlsRadioButton->isChecked()
            ? QnEmail::Tls
            : ui->sslRadioButton->isChecked()
              ? QnEmail::Ssl
              : QnEmail::Unsecure;
    if (result.port == 0)
        result.port = QnEmail::defaultPort(result.connectionType);
    result.simple = false;
    result.signature = ui->signatureLineEdit->text();
    result.supportEmail = ui->supportEmailLineEdit->text();
    return result;
}

void QnSmtpSettingsWidget::stopTesting(QString result) {
    m_timeoutTimer->stop();
    m_testHandle = -1;

    ui->testResultLabel->setText(result);
    ui->testProgressBar->setValue(ui->testProgressBar->maximum());
    ui->cancelTestButton->setVisible(false);
    ui->okTestButton->setVisible(true);
}

void QnSmtpSettingsWidget::loadSettings(QString server, QnEmail::ConnectionType connectionType, int port) {
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
        at_portComboBox_currentIndexChanged(ui->portComboBox->count());
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

void QnSmtpSettingsWidget::at_portComboBox_currentIndexChanged(int index) {
    int port = ui->portComboBox->itemData(index).toInt();
    if (port == QnEmail::defaultPort(QnEmail::Ssl)) {
        ui->tlsRecommendedLabel->hide();
        ui->sslRecommendedLabel->show();
    } else {
        ui->tlsRecommendedLabel->show();
        ui->sslRecommendedLabel->hide();
    }
}

void QnSmtpSettingsWidget::at_advancedCheckBox_toggled(bool toggled) {
    if (toggled) {
        QString value = ui->simpleEmailLineEdit->text();
        QnEmail email(value);

        QnEmail::SmtpServerPreset preset = email.smtpServer();
        if (preset.isNull()) {
            QString domain = email.domain();
            if (!domain.isEmpty())
                domain = QLatin1String("smtp.") + domain;
            loadSettings(domain, QnEmail::Unsecure);
        } else {
            loadSettings(preset.server, preset.connectionType, preset.port);
        }
        ui->userLineEdit->setText(value);
        ui->passwordLineEdit->setText(ui->simplePasswordLineEdit->text());
        ui->signatureLineEdit->setText(ui->simpleSignatureLineEdit->text());
        ui->supportEmailLineEdit->setText(ui->simpleSupportEmailLineEdit->text());
        validateEmailAdvanced();
    } else {
        ui->simpleEmailLineEdit->setText(ui->userLineEdit->text());
        ui->simplePasswordLineEdit->setText(ui->passwordLineEdit->text());
        ui->simpleSignatureLineEdit->setText(ui->signatureLineEdit->text());
        ui->simpleSupportEmailLineEdit->setText(ui->supportEmailLineEdit->text());
        validateEmailSimple();
    }
    ui->stackedWidget->setCurrentIndex(toggled ? AdvancedPage : SimplePage);
}

void QnSmtpSettingsWidget::at_testButton_clicked() {
    QnEmail::Settings result = settings();
    result.timeout = testSmtpTimeoutMSec / 1000;

    if (result.isNull()) {
        QMessageBox::warning(this, tr("Invalid data"), tr("Provided parameters are not valid. Could not perform a test."));
        return;
    }

    ui->controlsWidget->setEnabled(false);

    ui->testServerLabel->setText(result.server);
    ui->testPortLabel->setText(QString::number(result.port == 0
                                               ? QnEmail::defaultPort(result.connectionType)
                                               : result.port));
    ui->testUserLabel->setText(result.user);
    ui->testSecurityLabel->setText(result.connectionType == QnEmail::Tls
                                   ? tr("TLS")
                                   : result.connectionType == QnEmail::Ssl
                                     ? tr("SSL")
                                     : tr("Unsecured"));

    ui->cancelTestButton->setVisible(true);
    ui->okTestButton->setVisible(false);

    ui->testProgressBar->setValue(0);

    ui->testResultLabel->setText(tr("In Progress..."));

    m_timeoutTimer->setInterval(testSmtpTimeoutMSec / ui->testProgressBar->maximum());
    m_timeoutTimer->start();

    m_testHandle = QnAppServerConnectionFactory::getConnection2()->getBusinessEventManager()->testEmailSettings(
        result,
        this,
        [this](int handle, ec2::ErrorCode errorCode) {
            if (handle != m_testHandle)
                return;

            stopTesting(errorCode != ec2::ErrorCode::ok
                ? tr("Error while testing settings")
                : tr("Success") );
    });
    ui->stackedWidget->setCurrentIndex(TestingPage);
}

void QnSmtpSettingsWidget::at_cancelTestButton_clicked() {
    stopTesting(tr("Canceled"));
}

void QnSmtpSettingsWidget::at_timer_timeout() {
    int value = ui->testProgressBar->value();
    if (value < ui->testProgressBar->maximum()) {
        ui->testProgressBar->setValue(value + 1);
        return;
    }

    stopTesting(tr("Timed out"));
}

void QnSmtpSettingsWidget::at_okTestButton_clicked() {
    ui->controlsWidget->setEnabled(true);
    ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
                                       ? AdvancedPage
                                       : SimplePage);
}

void QnSmtpSettingsWidget::validateEmailSimple() {

    QString errorText;

    const QString targetEmail = ui->simpleEmailLineEdit->text();
    const QString supportEmail = ui->simpleSupportEmailLineEdit->text();

    if (!targetEmail.isEmpty()) {
        QnEmail email(targetEmail); 
        if (!email.isValid())
            errorText = tr("Email is not valid");
        else if (email.smtpServer().isNull())
            errorText = tr("No preset found. Use 'Advanced' option");
    }

    if (errorText.isEmpty() && !supportEmail.isEmpty()) {
        QnEmail support(supportEmail);
        if (!support.isValid())
            errorText = tr("Support email is not valid");
    } 

    ui->detectErrorLabel->setText(errorText);
}

void QnSmtpSettingsWidget::validateEmailAdvanced() {
    QString errorText;

    const QString supportEmail = ui->supportEmailLineEdit->text();

    if (!supportEmail.isEmpty()) {
        QnEmail support(supportEmail);
        if (!support.isValid())
            errorText = tr("Support email is not valid");
    }

    ui->supportEmailWarningLabel->setText(errorText);
}
