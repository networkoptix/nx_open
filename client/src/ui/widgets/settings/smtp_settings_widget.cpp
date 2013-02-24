#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtGui/QMessageBox>

#include <api/app_server_connection.h>

#include <ui/style/globals.h>

#include <utils/common/email.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/workbench/workbench_context.h>

//TODO: #GDM use documentation from http://support.google.com/mail/bin/answer.py?hl=en&answer=1074635


namespace {

    const QLatin1String nameFrom("EMAIL_FROM");
    const QLatin1String nameHost("EMAIL_HOST");
    const QLatin1String namePort("EMAIL_PORT");
    const QLatin1String nameUser("EMAIL_HOST_USER");
    const QLatin1String namePassword("EMAIL_HOST_PASSWORD");
    const QLatin1String nameTls("EMAIL_USE_TLS");
    const QLatin1String nameSsl("EMAIL_USE_SSL");
    const QLatin1String nameSimple("EMAIL_SIMPLE");
    const QLatin1String nameTimeout("EMAIL_TIMEOUT");

    const int TIMEOUT = 10;
}

QnSmtpSettingsWidget::QnSmtpSettingsWidget(QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSmtpSettingsWidget),
    m_requestHandle(-1),
    m_testHandle(-1),
    m_dns(NULL),
    m_autoMailServer(QString()),
    m_timeoutTimer(new QTimer(this)),
    m_settingsReceived(false)
{
    ui->setupUi(this);

    connect(ui->portComboBox,           SIGNAL(currentIndexChanged(int)),   this,   SLOT(at_portComboBox_currentIndexChanged(int)));
    connect(ui->advancedCheckBox,       SIGNAL(toggled(bool)),              this,   SLOT(at_advancedCheckBox_toggled(bool)));
    connect(ui->simpleEmailLineEdit,    SIGNAL(textChanged(QString)),       this,   SLOT(at_simpleEmail_textChanged(QString)));
    connect(ui->testButton,             SIGNAL(clicked()),                  this,   SLOT(at_testButton_clicked()));
    connect(ui->testResultButton,       SIGNAL(clicked()),                  this,   SLOT(at_testResultButton_clicked()));
    connect(m_timeoutTimer,             SIGNAL(timeout()),                  this,   SLOT(at_timer_timeout()));

    m_timeoutTimer->setInterval(100);
    m_timeoutTimer->setSingleShot(false);

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->detectErrorLabel->setPalette(palette);

    ui->testingProgressWidget->setVisible(false);

    for (int i = 0; i < QnEmail::ConnectionTypeCount; i++) {
        int port = QnEmail::defaultPort(static_cast<QnEmail::ConnectionType>(i));
        ui->portComboBox->addItem(QString::number(port), port);
    }
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
    bool ok = true;
    QnKvPairList pairs = settings(&ok);
    if (ok)
        QnAppServerConnectionFactory::createConnection()->saveSettingsAsync(pairs);
    //TODO: #GDM else?
}

QnKvPairList QnSmtpSettingsWidget::settings(bool *ok) {
    *ok = true;
    QnKvPairList result;

    bool simple = !ui->advancedCheckBox->isChecked();
    QnEmail::SmtpServerPreset preset;
    if (simple) {
        QnEmail email(ui->simpleEmailLineEdit->text());
        preset = email.smtpServer();
        if (preset.isNull()) {
            *ok = false;
            return result;
        }
    }

    QString hostname = simple ? preset.server : ui->serverLineEdit->text();
    int port = simple
            ? preset.port == 0
              ? QnEmail::defaultPort(preset.connectionType)
              : preset.port
            : ui->portComboBox->currentText().toInt();
    QString username = simple ? ui->simpleEmailLineEdit->text() : ui->userLineEdit->text();
    QString password = simple ? ui->simplePasswordLineEdit->text() : ui->passwordLineEdit->text();

    bool useTls = simple ? preset.connectionType == QnEmail::Tls : ui->tlsRadioButton->isChecked();
    bool useSsl = simple ? preset.connectionType == QnEmail::Ssl : ui->sslRadioButton->isChecked();

    result
        << QnKvPair(nameHost, hostname)
        << QnKvPair(namePort, port)
        << QnKvPair(nameUser, username)
        << QnKvPair(nameFrom, username)
        << QnKvPair(namePassword, password)
        << QnKvPair(nameTls, useTls)
        << QnKvPair(nameSsl, useSsl)
        << QnKvPair(nameSimple, simple)
        << QnKvPair(nameTimeout, TIMEOUT);
    return result;
}


void QnSmtpSettingsWidget::at_portComboBox_currentIndexChanged(int index) {

    int port = ui->portComboBox->itemData(index).toInt();
    if (port == QnEmail::defaultPort(QnEmail::Ssl)) {
        ui->sslRadioButton->setChecked(true);
        ui->tlsRecommendedLabel->hide();
        ui->sslRecommendedLabel->show();
    } else {
        ui->tlsRadioButton->setChecked(true);
        ui->tlsRecommendedLabel->show();
        ui->sslRecommendedLabel->hide();
    }
}

void QnSmtpSettingsWidget::at_testButton_clicked() {
    bool ok = true;
    QnKvPairList pairs = settings(&ok);
    if (!ok) {
        QMessageBox::warning(this, tr("Invalid data"), tr("Cannot test such parameters"));
        return;
    }

    ui->controlsWidget->setEnabled(false);
    ui->stackedWidget->setEnabled(false);
    ui->testResultButton->setText(tr("Cancel"));
    ui->testingProgressWidget->setVisible(true);

    ui->testProgressBar->setMaximum(TIMEOUT * 10); //timer interval is 100ms
    ui->testProgressBar->setFormat(QLatin1String("%p%"));
    ui->testProgressBar->setValue(0);
    m_timeoutTimer->start();

    m_testHandle = QnAppServerConnectionFactory::createConnection()->testEmailSettingsAsync(pairs,
                                                                                            this, SLOT(at_finishedTestEmailSettings(int, QByteArray, bool, int)));
}

void QnSmtpSettingsWidget::at_testResultButton_clicked() {
    m_timeoutTimer->stop();
    m_testHandle = -1;

    ui->controlsWidget->setEnabled(true);
    ui->stackedWidget->setEnabled(true);
    ui->testingProgressWidget->setVisible(false);
}

void QnSmtpSettingsWidget::at_timer_timeout() {
    int value = ui->testProgressBar->value();
    if (value < ui->testProgressBar->maximum()) {
        ui->testProgressBar->setValue(value + 1);
        return;
    }

    m_timeoutTimer->stop();
    m_testHandle = -1;
    ui->testProgressBar->setFormat(tr("Timeout"));
    ui->testResultButton->setText(tr("OK"));
}

void QnSmtpSettingsWidget::at_finishedTestEmailSettings(int status, const QByteArray &errorString, bool result, int handle) {
    if (handle != m_testHandle)
        return;

    result = result && (status == 0);
    if (status != 0) {
        //QMessageBox::critical(this, tr("Error while testing settings"), QString::fromLatin1(errorString));
    }

    qDebug() << status << errorString << result << handle;

    m_timeoutTimer->stop();
    m_testHandle = -1;
    ui->testProgressBar->setValue(ui->testProgressBar->maximum());
    ui->testProgressBar->setFormat(result ? tr("Success") : tr("Error"));
    ui->testResultButton->setText(tr("OK"));

}

void QnSmtpSettingsWidget::at_settings_received(int status, const QByteArray &errorString, const QnKvPairList &settings, int handle) {
    Q_UNUSED(errorString)
    if (handle != m_requestHandle)
        return;

    m_requestHandle = -1;

    bool success = (status == 0);
    m_settingsReceived = true;
    if(!success) {
        //TODO: #GDM remove password from error message
        //QMessageBox::critical(this, tr("Error while receiving settings"), QString::fromLatin1(errorString));
        return;
    }

    bool useTls = true;
    bool useSsl = false;

    foreach (const QnKvPair &setting, settings) {
        if (setting.name() == nameHost) {
            ui->serverLineEdit->setText(setting.value());
        } else if (setting.name() == namePort) {
            int port = setting.value().toInt();
            bool found = false;
            for (int i = 0; i < ui->portComboBox->count(); i++) {
                if (ui->portComboBox->itemData(i).toInt() == port) {
                    ui->portComboBox->setCurrentIndex(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                ui->portComboBox->setEditText(QString::number(port));
                at_portComboBox_currentIndexChanged(ui->portComboBox->count());
            }
        } else if (setting.name() == nameUser) {
            ui->userLineEdit->setText(setting.value());
            ui->simpleEmailLineEdit->setText(setting.value());
        } else if (setting.name() == namePassword) {
            ui->passwordLineEdit->setText(setting.value());
            ui->simplePasswordLineEdit->setText(setting.value());
        } else if (setting.name() == nameTls) {
            useTls = setting.value() == QLatin1String("True");
        } else if (setting.name() == nameSsl) {
            useSsl = setting.value() == QLatin1String("True");
        } else if (setting.name() == nameSimple) {
            bool simple = setting.value() == QLatin1String("True");
            ui->advancedCheckBox->setChecked(!simple);
        }
    }

    if (useTls)
        ui->tlsRadioButton->setChecked(true);
    else if (useSsl)
        ui->sslRadioButton->setChecked(true);
    else
        ui->unsecuredRadioButton->setChecked(true);

}


void QnSmtpSettingsWidget::at_simpleEmail_textChanged(const QString &value) {
    ui->detectErrorLabel->setText(QString());

    QnEmail email(value);
    if (!email.isValid()) {
        ui->detectErrorLabel->setText(tr("Email is not valid"));
        return;
    }
    QnEmail::SmtpServerPreset preset = email.smtpServer();
    m_autoMailServer = preset.server;
    if (m_autoMailServer.isEmpty()) {
        ui->detectErrorLabel->setText(tr("No preset found. Use 'Advanced' option."));
    } else if (!ui->advancedCheckBox->isChecked()) {
        ui->serverLineEdit->setText(m_autoMailServer);
    }
}

void QnSmtpSettingsWidget::at_advancedCheckBox_toggled(bool toggled) {
    ui->stackedWidget->setCurrentIndex(toggled ? 1 : 0);
    /*ui->serverLineEdit->setEnabled(!toggled);
    ui->userLineEdit->setEnabled(!toggled);
    ui->portComboBox->setEnabled(!toggled);
    ui->tlsRadioButton->setEnabled(!toggled);
    ui->unsecuredRadioButton->setEnabled(!toggled);

    if (toggled && !m_autoMailServer.isEmpty()) {
        ui->serverLineEdit->setText(m_autoMailServer);
        ui->userLineEdit->setText(context()->user()->getEmail().trimmed());
        ui->portComboBox->setCurrentIndex(0);
        ui->tlsRadioButton->setChecked(true);
        //TODO: #GDM setup other smtp settings
    }*/
}

