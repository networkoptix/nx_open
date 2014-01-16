#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>

#include <ui/style/warning_style.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/handlers/workbench_notifications_handler.h>

namespace {
    enum WidgetPages {
        SimplePage,
        AdvancedPage,
        TestingPage
    };
}

QValidator::State QnPortNumberValidator::validate(QString &input, int &pos) const {
    // TODO: #GDM #TR Logic on translated strings? And if it is translated differently in different context? Overall this is evil. 
    // You should consider translated strings as objects with only one operation: showToUser.
    if (input == tr("Auto")) 
        return QValidator::Acceptable;

    QValidator::State result = base_type::validate(input, pos);
    if (result == QValidator::Acceptable &&
            (input.toInt() == 0 || input.toInt() > 65535)
            )
        return QValidator::Intermediate;
    return result;
}

void QnPortNumberValidator::fixup(QString &input) const {
    if (input == tr("Auto"))
        return;
    if (input.toInt() == 0 || input.toInt() > 65535)
        input = tr("Auto");
}

QnSmtpSettingsWidget::QnSmtpSettingsWidget(QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::SmtpSettingsWidget),
    m_requestHandle(-1),
    m_testHandle(-1),
    m_timeoutTimer(new QTimer(this)),
    m_settingsReceived(false)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Server_Mail_Help);

    connect(ui->portComboBox,           SIGNAL(currentIndexChanged(int)),   this,   SLOT(at_portComboBox_currentIndexChanged(int)));
    connect(ui->advancedCheckBox,       SIGNAL(toggled(bool)),              this,   SLOT(at_advancedCheckBox_toggled(bool)));
    connect(ui->simpleEmailLineEdit,    SIGNAL(textChanged(QString)),       this,   SLOT(at_simpleEmail_textChanged(QString)));
    connect(ui->testButton,             SIGNAL(clicked()),                  this,   SLOT(at_testButton_clicked()));
    connect(ui->cancelTestButton,       SIGNAL(clicked()),                  this,   SLOT(at_cancelTestButton_clicked()));
    connect(ui->okTestButton,           SIGNAL(clicked()),                  this,   SLOT(at_okTestButton_clicked()));
    connect(m_timeoutTimer,             SIGNAL(timeout()),                  this,   SLOT(at_timer_timeout()));

    m_timeoutTimer->setSingleShot(false);

    setWarningStyle(ui->detectErrorLabel);

    ui->portComboBox->addItem(tr("Auto"), 0);
    for (int i = 0; i < QnEmail::ConnectionTypeCount; i++) {
        int port = QnEmail::defaultPort(static_cast<QnEmail::ConnectionType>(i));
        ui->portComboBox->addItem(QString::number(port), port);
    }
    ui->portComboBox->setValidator(new QnPortNumberValidator(this));
//    ui->portComboBox->setCurrentIndex(0);
//    at_portComboBox_currentIndexChanged(ui->portComboBox->currentIndex());
}

QnSmtpSettingsWidget::~QnSmtpSettingsWidget()
{
}

void QnSmtpSettingsWidget::updateFromSettings() {
    m_settingsReceived = false;

    m_requestHandle = QnAppServerConnectionFactory::createConnection()->getSettingsAsync(
                this, SLOT(at_settings_received(int, const QnKvPairList&, int)));
}

void QnSmtpSettingsWidget::submitToSettings() {
    QnEmail::Settings result = settings();
    QnAppServerConnectionFactory::createConnection()->saveSettingsAsync(result.serialized(),
                                                                        context()->instance<QnWorkbenchNotificationsHandler>(),
                                                                        SLOT(updateSmtpSettings(int, const QnKvPairList&, int)));
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

void QnSmtpSettingsWidget::at_simpleEmail_textChanged(const QString &value) {
    if (!m_settingsReceived)
        return;

    ui->detectErrorLabel->setText(QString());

    if (value.isEmpty())
        return;

    QnEmail email(value);
    if (!email.isValid()) {
        ui->detectErrorLabel->setText(tr("Email is not valid"));
    } else
    if (email.smtpServer().isNull()) {
        ui->detectErrorLabel->setText(tr("No preset found. Use 'Advanced' option."));
    }
}

void QnSmtpSettingsWidget::at_advancedCheckBox_toggled(bool toggled) {
    if (!m_settingsReceived)
        return;

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
    } else {
        ui->simpleEmailLineEdit->setText(ui->userLineEdit->text());
        ui->simplePasswordLineEdit->setText(ui->passwordLineEdit->text());
        ui->simpleSignatureLineEdit->setText(ui->signatureLineEdit->text());
        at_simpleEmail_textChanged(ui->userLineEdit->text()); //force error checking
    }
    ui->stackedWidget->setCurrentIndex(toggled ? AdvancedPage : SimplePage);
}

void QnSmtpSettingsWidget::at_testButton_clicked() {
    if (!m_settingsReceived)
        return;

    QnEmail::Settings result = settings();
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

    m_timeoutTimer->setInterval(result.timeout * 1000 / ui->testProgressBar->maximum());
    m_timeoutTimer->start();

    m_testHandle = QnAppServerConnectionFactory::createConnection()->testEmailSettingsAsync(result.serialized(),
                                                                                            this, SLOT(at_finishedTestEmailSettings(int, bool, int)));
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

void QnSmtpSettingsWidget::at_finishedTestEmailSettings(int status, bool result, int handle) {
    if (handle != m_testHandle)
        return;

    stopTesting(status != 0
            ? tr("Error while testing settings")
            : result
              ? tr("Success")
              : tr("Error")
                );
}

void QnSmtpSettingsWidget::at_okTestButton_clicked() {
    ui->controlsWidget->setEnabled(true);
    ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
                                       ? AdvancedPage
                                       : SimplePage);
}

void QnSmtpSettingsWidget::at_settings_received(int status, const QnKvPairList &values, int handle) {
    if (handle != m_requestHandle)
        return;

    m_requestHandle = -1;
    context()->instance<QnWorkbenchNotificationsHandler>()->updateSmtpSettings(status, values, handle);

    bool success = (status == 0);
    if(!success) {
        QMessageBox::critical(this, tr("Error"), tr("Could not read settings from Enterprise Controller."));
        m_settingsReceived = true;
        return;
    }

    QnEmail::Settings settings(values);
    loadSettings(settings.server, settings.connectionType, settings.port);
    ui->userLineEdit->setText(settings.user);
    ui->simpleEmailLineEdit->setText(settings.user);
    ui->passwordLineEdit->setText(settings.password);
    ui->simplePasswordLineEdit->setText(settings.password);
    ui->signatureLineEdit->setText(settings.signature);
    ui->simpleSignatureLineEdit->setText(settings.signature);
    ui->advancedCheckBox->setChecked(!settings.simple);
    ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
                                       ? AdvancedPage
                                       : SimplePage);
    m_settingsReceived = true;
    updateFocusedElement();
}
