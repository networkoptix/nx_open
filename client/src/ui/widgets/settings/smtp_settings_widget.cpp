#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtGui/QMessageBox>
#include <qt5/network/qdnslookup.h>

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
    const QLatin1String nameSimple("EMAIL_SIMPLE");

    const int PORT_UNSECURE = 25;
    const int PORT_TLS = 587;

}

QnSmtpSettingsWidget::QnSmtpSettingsWidget(QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSmtpSettingsWidget),
    m_requestHandle(-1),
    m_dns(NULL),
    m_autoMailServer(QString()),
    m_settingsReceived(false),
    m_mailServersReceived(false)
{
    ui->setupUi(this);

    connect(ui->portComboBox,       SIGNAL(currentIndexChanged(int)),   this,   SLOT(at_portComboBox_currentIndexChanged(int)));
    connect(ui->advancedCheckBox,   SIGNAL(toggled(bool)),          this,   SLOT(at_advancedCheckBox_toggled(bool)));
    connect(ui->simpleEmailLineEdit, SIGNAL(editingFinished()), this, SLOT(at_simpleEmail_editingFinished()));
//    connect(ui->testButton,         SIGNAL(clicked()),                  this,   SLOT(at_testButton_clicked()));

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->detectErrorLabel->setPalette(palette);

    ui->portComboBox->addItem(QString::number(PORT_TLS));
    ui->portComboBox->addItem(QString::number(PORT_UNSECURE));
    ui->portComboBox->setCurrentIndex(0);
    at_portComboBox_currentIndexChanged(ui->portComboBox->currentIndex());
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

    bool simple = !ui->advancedCheckBox->isChecked();
    QString hostname = simple ? m_autoMailServer : ui->serverLineEdit->text();
    int port = simple ? PORT_UNSECURE
                      : ui->portComboBox->currentIndex() == 0 ? PORT_TLS : PORT_UNSECURE;
    QString username = simple ? ui->simpleEmailLineEdit->text() : ui->userLineEdit->text();
    QString password = simple ? ui->simplePasswordLineEdit->text() : ui->passwordLineEdit->text();
    bool useTls = simple ? false : ui->tlsRadioButton->isChecked();

    QnKvPairList settings;
    settings
        << QnKvPair(nameHost, hostname)
        << QnKvPair(namePort, QString::number(port))
        << QnKvPair(nameUser, username)
        << QnKvPair(nameFrom, username)
        << QnKvPair(namePassword, password)
        << QnKvPair(nameTls, useTls ? QLatin1String("True") : QLatin1String("False"))
        << QnKvPair(nameSimple, simple ? QLatin1String("True") : QLatin1String("False"));

    QnAppServerConnectionFactory::createConnection()->saveSettingsAsync(settings);
}

void QnSmtpSettingsWidget::updateMailServers() {
    m_mailServersReceived = false;
    m_autoMailServer = QString();
    ui->detectErrorLabel->setText(QString());

    QString email = ui->simpleEmailLineEdit->text();
    if (isEmailValid(email)) {
        QString hostname = getEmailDomain(email);

        m_dns = new QDnsLookup(this);
        connect(m_dns, SIGNAL(finished()), this, SLOT(at_mailServers_received()));

        m_dns->setType(QDnsLookup::MX);
        m_dns->setName(hostname);
        m_dns->lookup();
    }
    else {
        m_mailServersReceived = true;
        ui->detectErrorLabel->setText(tr("Email is not valid"));
    }
}

void QnSmtpSettingsWidget::at_portComboBox_currentIndexChanged(int index) {

    /*
     * index = 0: port 587, default for TLS
     * index = 1: port 25,  default for unsecured, often supports TLS
     */

    if (index == 0)
        ui->tlsRadioButton->setChecked(true);
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

    foreach (const QnKvPair &setting, settings) {
        if (setting.name() == nameHost) {
            ui->serverLineEdit->setText(setting.value());
        } else if (setting.name() == namePort) {
            bool ok;
            int port = setting.value().toInt(&ok);
            int idx = (ok && port == PORT_UNSECURE) ? 1 : 0;
            ui->portComboBox->setCurrentIndex(idx);
        } else if (setting.name() == nameUser) {
            ui->userLineEdit->setText(setting.value());
            ui->simpleEmailLineEdit->setText(setting.value());
        } else if (setting.name() == namePassword) {
            ui->passwordLineEdit->setText(setting.value());
            ui->simplePasswordLineEdit->setText(setting.value());
        } else if (setting.name() == nameTls) {
            bool useTls = setting.value() == QLatin1String("True");
            if (useTls)
                ui->tlsRadioButton->setChecked(true);
            else
                ui->unsecuredRadioButton->setChecked(true);
        } else if (setting.name() == nameSimple) {
            bool simple = setting.value() == QLatin1String("True");
            ui->advancedCheckBox->setChecked(!simple);
            if (simple)
                updateMailServers();
        }
    }
}

void QnSmtpSettingsWidget::at_mailServers_received() {
    m_mailServersReceived = true;

    if (m_dns->error() != QDnsLookup::NoError) {
        ui->detectErrorLabel->setText(tr("DNS lookup failed"));
        m_dns->deleteLater();
        return;
    }

    int preferred = -1;
    // Handle the results.
    foreach (const QDnsMailExchangeRecord &record, m_dns->mailExchangeRecords()) {
        if (preferred < 0 || record.preference() < preferred) {
            preferred = record.preference();
            m_autoMailServer = record.exchange();
        }
    }
    m_dns->deleteLater();

    if (m_autoMailServer.isEmpty()) {
        ui->detectErrorLabel->setText(tr("No mail servers found."));
    } else if (!ui->advancedCheckBox->isChecked()) {
        ui->serverLineEdit->setText(m_autoMailServer);
    }
}

void QnSmtpSettingsWidget::at_simpleEmail_editingFinished() {
    updateMailServers();
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

