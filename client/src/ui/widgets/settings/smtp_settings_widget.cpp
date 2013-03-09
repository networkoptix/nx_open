#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtGui/QMessageBox>

#include <api/app_server_connection.h>

#include <ui/style/warning_style.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/workbench/workbench_context.h>

//TODO: #GDM use documentation from http://support.google.com/mail/bin/answer.py?hl=en&answer=1074635


namespace {
    enum WidgetPages {
        SimplePage,
        AdvancedPage,
        TestingPage
    };
}

QValidator::State QnPortNumberValidator::validate(QString &input, int &pos) const {
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
    ui(new Ui::QnSmtpSettingsWidget),
    m_requestHandle(-1),
    m_testHandle(-1),
    m_timeoutTimer(new QTimer(this)),
    m_settingsReceived(false)
{
    ui->setupUi(this);

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

void QnSmtpSettingsWidget::update() {
    m_settingsReceived = false;

    m_requestHandle = QnAppServerConnectionFactory::createConnection()->getSettingsAsync(
                this, SLOT(at_settings_received(int,QByteArray,QnKvPairList,int)));
}

void QnSmtpSettingsWidget::submit() {
    QnEmail::Settings result = settings();
    //if (!result.isNull())
        QnAppServerConnectionFactory::createConnection()->saveSettingsAsync(result.serialized());
    //TODO: #GDM else?
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
    } else {
        ui->simpleEmailLineEdit->setText(ui->userLineEdit->text());
        ui->simplePasswordLineEdit->setText(ui->passwordLineEdit->text());
    }
    ui->stackedWidget->setCurrentIndex(toggled ? AdvancedPage : SimplePage);
}

void QnSmtpSettingsWidget::at_testButton_clicked() {
    QnEmail::Settings result = settings();
    if (result.isNull()) {
        QMessageBox::warning(this, tr("Invalid data"), tr("Cannot test such parameters"));
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
                                                                                            this, SLOT(at_finishedTestEmailSettings(int, QByteArray, bool, int)));
    ui->stackedWidget->setCurrentIndex(TestingPage);
}

void QnSmtpSettingsWidget::at_cancelTestButton_clicked() {
    stopTesting(tr("Cancelled"));
}

void QnSmtpSettingsWidget::at_timer_timeout() {
    int value = ui->testProgressBar->value();
    if (value < ui->testProgressBar->maximum()) {
        ui->testProgressBar->setValue(value + 1);
        return;
    }

    stopTesting(tr("Timeout"));
}

void QnSmtpSettingsWidget::at_finishedTestEmailSettings(int status, const QByteArray &errorString, bool result, int handle) {
    if (handle != m_testHandle)
        return;

    stopTesting(status != 0
            ? tr("Error while testing settings")
            : result
              ? tr("Success")
              : tr("Error") + QString::fromLatin1(errorString)
                );
}

void QnSmtpSettingsWidget::at_okTestButton_clicked() {
    ui->controlsWidget->setEnabled(true);
    ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
                                       ? AdvancedPage
                                       : SimplePage);
}

void QnSmtpSettingsWidget::at_settings_received(int status, const QByteArray &errorString, const QnKvPairList &values, int handle) {
    Q_UNUSED(errorString)
    if (handle != m_requestHandle)
        return;

    m_requestHandle = -1;

    bool success = (status == 0);
    if(!success) {
        //TODO: #GDM remove password from error message
        //QMessageBox::critical(this, tr("Error while receiving settings"), QString::fromLatin1(errorString));
        m_settingsReceived = true;
        return;
    }

    QnEmail::Settings settings(values);
    loadSettings(settings.server, settings.connectionType, settings.port);
    ui->userLineEdit->setText(settings.user);
    ui->simpleEmailLineEdit->setText(settings.user);
    ui->passwordLineEdit->setText(settings.password);
    ui->simplePasswordLineEdit->setText(settings.password);
    ui->advancedCheckBox->setChecked(!settings.simple);
    ui->stackedWidget->setCurrentIndex(ui->advancedCheckBox->isChecked()
                                       ? AdvancedPage
                                       : SimplePage);

    m_settingsReceived = true;
}
