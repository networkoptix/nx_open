#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include "api/model/test_email_settings_reply.h"

#include <nx_ec/ec_api.h>

#include <ui/style/warning_style.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/app_info.h>
#include <utils/common/email.h>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"

namespace {
    enum WidgetPages {
        SimplePage,
        AdvancedPage,
        TestingPage
    };

    QList<QnEmail::ConnectionType> connectionTypesAllowed() {
        return QList<QnEmail::ConnectionType>() 
            << QnEmail::Unsecure
            << QnEmail::Ssl
            << QnEmail::Tls;
    }

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
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SmtpSettingsWidget),
    m_testHandle(-1),
    m_timeoutTimer(new QTimer(this))
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Server_Mail_Help);

    connect(ui->portComboBox,               QnComboboxCurrentIndexChanged,      this,   &QnSmtpSettingsWidget::at_portComboBox_currentIndexChanged);
    connect(ui->advancedCheckBox,           &QCheckBox::toggled,                this,   &QnSmtpSettingsWidget::at_advancedCheckBox_toggled);
    connect(ui->testButton,                 &QPushButton::clicked,              this,   &QnSmtpSettingsWidget::at_testButton_clicked);
    connect(ui->cancelTestButton,           &QPushButton::clicked,              this,   &QnSmtpSettingsWidget::at_cancelTestButton_clicked);
    connect(ui->okTestButton,               &QPushButton::clicked,              this,   &QnSmtpSettingsWidget::finishTesting);
    connect(m_timeoutTimer,                 &QTimer::timeout,                   this,   &QnSmtpSettingsWidget::at_timer_timeout);
    connect(ui->simpleEmailLineEdit,        &QLineEdit::textChanged,            this,   &QnSmtpSettingsWidget::validateEmailSimple);
    connect(ui->simpleSupportEmailLineEdit, &QLineEdit::textChanged,            this,   &QnSmtpSettingsWidget::validateEmailSimple);
    connect(ui->emailLineEdit,              &QLineEdit::textChanged,            this,   &QnSmtpSettingsWidget::validateEmailAdvanced);
    connect(ui->supportEmailLineEdit,       &QLineEdit::textChanged,            this,   &QnSmtpSettingsWidget::validateEmailAdvanced);
    connect(QnGlobalSettings::instance(),   &QnGlobalSettings::emailSettingsChanged, this,  &QnSmtpSettingsWidget::updateFromSettings);

    m_timeoutTimer->setSingleShot(false);

    setWarningStyle(ui->detectErrorLabel);
    setWarningStyle(ui->supportEmailWarningLabel);

    const QString autoPort = tr("Auto");
    ui->portComboBox->addItem(autoPort, 0);
    for (QnEmail::ConnectionType type: connectionTypesAllowed()) {
        int port = QnEmailSettings::defaultPort(type);
        ui->portComboBox->addItem(QString::number(port), port);
    }
    ui->portComboBox->setValidator(new QnPortNumberValidator(autoPort, this));

    ui->supportEmailLineEdit->setPlaceholderText(QnAppInfo::supportAddress());
    ui->simpleSupportEmailLineEdit->setPlaceholderText(QnAppInfo::supportAddress());
}

QnSmtpSettingsWidget::~QnSmtpSettingsWidget()
{
}

void QnSmtpSettingsWidget::updateFromSettings() {
    finishTesting();

    QnEmailSettings settings = QnGlobalSettings::instance()->emailSettings();

    loadSettings(settings.server, settings.connectionType, settings.port);
    ui->userLineEdit->setText(settings.user);
    ui->emailLineEdit->setText(settings.email);
    ui->simpleEmailLineEdit->setText(settings.email);
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
}

void QnSmtpSettingsWidget::submitToSettings() {
    QnGlobalSettings::instance()->setEmailSettings(settings());
}

bool QnSmtpSettingsWidget::confirm() {
    finishTesting();
    return base_type::confirm();
}

bool QnSmtpSettingsWidget::discard() {
    finishTesting();
    return base_type::discard();
}

QnEmailSettings QnSmtpSettingsWidget::settings() const {

    QnEmailSettings result;

    if (!ui->advancedCheckBox->isChecked()) {
        QnEmailAddress email(ui->simpleEmailLineEdit->text());
        result = email.settings();
        result.email = ui->simpleEmailLineEdit->text(); 
        result.user = result.email;
        result.password = ui->simplePasswordLineEdit->text();
        result.simple = true;
        result.signature = ui->simpleSignatureLineEdit->text();
        result.supportEmail = ui->simpleSupportEmailLineEdit->text();
        return result;
    }

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
    if (result.port == 0)
        result.port = QnEmailSettings::defaultPort(result.connectionType);
    result.simple = false;
    result.signature = ui->signatureLineEdit->text();
    result.supportEmail = ui->supportEmailLineEdit->text();
    return result;
}

void QnSmtpSettingsWidget::stopTesting(const QString &result) {
    if (m_testHandle < 0)
        return;
    m_timeoutTimer->stop();
    m_testHandle = -1;

    ui->testResultLabel->setText(result);
    ui->testProgressBar->setValue(ui->testProgressBar->maximum());
    ui->cancelTestButton->setVisible(false);
    ui->okTestButton->setVisible(true);
}

void QnSmtpSettingsWidget::finishTesting() {
    stopTesting();
    ui->controlsWidget->setEnabled(true);
    if (ui->stackedWidget->currentIndex() == TestingPage)
        ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
            ? AdvancedPage
            : SimplePage);
}

void QnSmtpSettingsWidget::loadSettings(const QString &server, QnEmail::ConnectionType connectionType, int port) {
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
    if (port == QnEmailSettings::defaultPort(QnEmail::Ssl)) {
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
        QnEmailAddress email(value);

        QnEmailSmtpServerPreset preset = email.smtpServer();
        if (preset.isNull()) {
            QString domain = email.domain();
            if (!domain.isEmpty())
                domain = QLatin1String("smtp.") + domain;
            loadSettings(domain, QnEmail::Unsecure);
        } else {
            loadSettings(preset.server, preset.connectionType, preset.port);
        }
        ui->userLineEdit->setText(email.user());
        ui->emailLineEdit->setText(value);
        ui->passwordLineEdit->setText(ui->simplePasswordLineEdit->text());
        ui->signatureLineEdit->setText(ui->simpleSignatureLineEdit->text());
        ui->supportEmailLineEdit->setText(ui->simpleSupportEmailLineEdit->text());
        validateEmailAdvanced();
    } else {
        ui->simpleEmailLineEdit->setText(ui->emailLineEdit->text());
        ui->simplePasswordLineEdit->setText(ui->passwordLineEdit->text());
        ui->simpleSignatureLineEdit->setText(ui->signatureLineEdit->text());
        ui->simpleSupportEmailLineEdit->setText(ui->supportEmailLineEdit->text());
        validateEmailSimple();
    }
    ui->stackedWidget->setCurrentIndex(toggled ? AdvancedPage : SimplePage);
}

void QnSmtpSettingsWidget::at_testButton_clicked() {
    QnEmailSettings result = settings();
    result.timeout = testSmtpTimeoutMSec / 1000;

    if (result.isNull()) {
        QMessageBox::warning(this, tr("Invalid data"), tr("Provided parameters are not valid. Could not perform a test."));
        return;
    }

    QnMediaServerResourcePtr serverResource;
    QnMediaServerResourceList servers = qnResPool->getAllServers();
    if (!servers.isEmpty()) {
        serverResource = servers.first();
        foreach(QnMediaServerResourcePtr mserver, servers)
        {
            if (mserver->getServerFlags() & Qn::SF_HasPublicIP) {
                serverResource = mserver;
                break;
            }
        }
    }
    QnMediaServerConnectionPtr serverConnection = serverResource
        ? serverResource->apiConnection()
        : QnMediaServerConnectionPtr();
    if (!serverConnection) {
        QMessageBox::warning(this, tr("Network Error"), tr("Could not perform a test. None of your servers has a public IP."));
        return;
    }

    ui->controlsWidget->setEnabled(false);

    ui->testServerLabel->setText(result.server);
    ui->testPortLabel->setText(QString::number(result.port == 0
                                               ? QnEmailSettings::defaultPort(result.connectionType)
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

    ui->stackedWidget->setCurrentIndex(TestingPage);
    m_testHandle = serverConnection->testEmailSettingsAsync( result, this, SLOT(at_testEmailSettingsFinished(int, const QnTestEmailSettingsReply& , int)));       
}

void QnSmtpSettingsWidget::at_testEmailSettingsFinished(int status, const QnTestEmailSettingsReply& reply, int handle)
{
    if (handle != m_testHandle)
        return;

    stopTesting(status != 0 || reply.errorCode != 0
        ? tr("Error while testing settings")
        : tr("Success") );
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

void QnSmtpSettingsWidget::validateEmailSimple() {

    QString errorText;

    const QString targetEmail = ui->simpleEmailLineEdit->text();
    const QString supportEmail = ui->simpleSupportEmailLineEdit->text();

    if (!targetEmail.isEmpty()) {
        QnEmailAddress email(targetEmail); 
        if (!email.isValid())
            errorText = tr("Email is not valid");
        else if (email.smtpServer().isNull())
            errorText = tr("No preset found. Use 'Advanced' option");
    }

    if (errorText.isEmpty() && !supportEmail.isEmpty()) {
        QnEmailAddress support(supportEmail);
        if (!support.isValid())
            errorText = tr("Support email is not valid");
    } 

    ui->detectErrorLabel->setText(errorText);
}

void QnSmtpSettingsWidget::validateEmailAdvanced() {
    QString errorText;

    const QString targetEmail = ui->emailLineEdit->text();
    const QString supportEmail = ui->supportEmailLineEdit->text();

    if (!targetEmail.isEmpty()) {
        QnEmailAddress email(targetEmail); 
        if (!email.isValid())
            errorText = tr("Email is not valid");
    }

    if (errorText.isEmpty() && !supportEmail.isEmpty()) {
        QnEmailAddress support(supportEmail);
        if (!support.isValid())
            errorText = tr("Support email is not valid");
    }

    ui->supportEmailWarningLabel->setText(errorText);
}

bool QnSmtpSettingsWidget::hasChanges() const  {
    return settings() != QnGlobalSettings::instance()->emailSettings();
}

