#include "smtp_settings_widget.h"
#include "ui_smtp_settings_widget.h"

#include <QtGui/QMessageBox>
#include <qt5/network/qdnslookup.h>

#include <api/app_server_connection.h>

#include <core/resource_managment/resource_pool.h>

#include <ui/style/globals.h>

#include <utils/settings.h>
#include <utils/common/email.h>

//TODO: #GDM use documentation from http://support.google.com/mail/bin/answer.py?hl=en&answer=1074635

namespace {

    const QLatin1String nameFrom("EMAIL_FROM");
    const QLatin1String nameHost("EMAIL_HOST");
    const QLatin1String namePort("EMAIL_PORT");
    const QLatin1String nameUser("EMAIL_HOST_USER");
    const QLatin1String namePassword("EMAIL_HOST_PASSWORD");
    const QLatin1String nameTls("EMAIL_USE_TLS");

}

QnSmtpSettingsWidget::QnSmtpSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnSmtpSettingsWidget),
    m_requestHandle(-1),
    m_dns(NULL),
    m_autoMailServer(QString()),
    m_settingsReceived(false),
    m_mailServersReceived(false)
{
    ui->setupUi(this);

    connect(ui->portComboBox,   SIGNAL(currentIndexChanged(int)),   this,   SLOT(at_portComboBox_currentIndexChanged(int)));
    connect(ui->autoDetectCheckBox, SIGNAL(toggled(bool)),          this,   SLOT(at_autoDetectCheckBox_toggled(bool)));
    connect(ui->testButton,     SIGNAL(clicked()),                  this,   SLOT(at_testButton_clicked()));

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->detectErrorLabel->setPalette(palette);

    at_portComboBox_currentIndexChanged(ui->portComboBox->currentIndex());
}

QnSmtpSettingsWidget::~QnSmtpSettingsWidget()
{
}

void QnSmtpSettingsWidget::update() {
    m_settingsReceived = false;
    m_mailServersReceived = false;

    m_requestHandle = QnAppServerConnectionFactory::createConnection()->getSettingsAsync(
                this, SLOT(at_settings_received(int,QByteArray,QnKvPairList,int)));

    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (admin && isEmailValid(admin->getEmail().trimmed())) {
        QString hostname = getEmailDomain(admin->getEmail());

        m_dns = new QDnsLookup(this);
        connect(m_dns, SIGNAL(finished()), this, SLOT(at_mailServers_received()));

        m_dns->setType(QDnsLookup::MX);
        m_dns->setName(hostname);
        m_dns->lookup();

        m_autoUser = getEmailUser(admin->getEmail());
    }
    else {
        m_mailServersReceived = true;
        if (!admin)
            qWarning() << "Admin is not found";
        ui->detectErrorLabel->setText(tr("Administrator email is not valid"));
    }
    updateOverlay();
}

void QnSmtpSettingsWidget::submit() {
    bool useTls = ui->tlsRadioButton->isChecked();
    int port = ui->portComboBox->currentIndex() == 0 ? 587 : 25;

    QnKvPairList settings;
    settings
        << QnKvPair(nameHost, ui->serverLineEdit->text())
        << QnKvPair(namePort, QString::number(port))
        << QnKvPair(nameUser, ui->userLineEdit->text())
        << QnKvPair(nameFrom, ui->userLineEdit->text())
        << QnKvPair(namePassword, ui->passwordLineEdit->text())
        << QnKvPair(nameTls, useTls ? QLatin1String("True") : QLatin1String("False"));

    QnAppServerConnectionFactory::createConnection()->saveSettingsAsync(settings);
}

void QnSmtpSettingsWidget::updateOverlay() {
    if (!m_settingsReceived || !m_mailServersReceived)
        return; //TODO: #GDM disable all, draw progress bar

    bool autoDetect = qnSettings->autoDetectSmtp() &&
            !m_autoMailServer.isEmpty();
//            &&  ui->serverLineEdit->text() == m_autoMailServer;
    ui->autoDetectCheckBox->setEnabled(!m_autoMailServer.isEmpty());
    ui->autoDetectCheckBox->setChecked(autoDetect);



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
        updateOverlay();
        return;
    }

    foreach (const QnKvPair &setting, settings) {
        if (setting.name() == nameHost) {
            ui->serverLineEdit->setText(setting.value());
        } else if (setting.name() == namePort) {
            bool ok;
            int port = setting.value().toInt(&ok);
            int idx = (ok && port == 25) ? 1 : 0;
            ui->portComboBox->setCurrentIndex(idx);
        } else if (setting.name() == nameUser) {
            ui->userLineEdit->setText(setting.value());
        } else if (setting.name() == namePassword) {
            ui->passwordLineEdit->setText(setting.value());
        } else if (setting.name() == nameTls) {
            bool useTls = setting.value() == QLatin1String("True");
            if (useTls)
                ui->tlsRadioButton->setChecked(true);
            else
                ui->unsecuredRadioButton->setChecked(true);
        }
    }

    updateOverlay();
}

/*
\value ResolverError        there was an error initializing the system's DNS resolver.

\value OperationCancelledError  the lookup was aborted using the abort() method.

\value InvalidRequestError  the requested DNS lookup was invalid.

\value InvalidReplyError    the reply returned by the server was invalid.

\value ServerFailureError   the server encountered an internal failure while processing the request (SERVFAIL).

\value ServerRefusedError   the server refused to process the request for security or policy reasons (REFUSED).

\value NotFoundError        the requested domain name does not exist (NXDOMAIN).
*/

void QnSmtpSettingsWidget::at_mailServers_received() {
    m_mailServersReceived = true;
    m_autoMailServer = QString();

    if (m_dns->error() != QDnsLookup::NoError) {
        ui->detectErrorLabel->setText(tr("DNS lookup failed"));
        m_dns->deleteLater();
        updateOverlay();
        return;
    }

    int preferred = -1;
    // Handle the results.
    foreach (const QDnsMailExchangeRecord &record, m_dns->mailExchangeRecords()) {
        if (preferred < 0 || record.preference() < preferred) {
            preferred = record.preference();
            m_autoMailServer = record.exchange();
        }
        qDebug() << record.exchange() << record.preference();
    }
    m_dns->deleteLater();

    if (m_autoMailServer.isEmpty()) {
        ui->detectErrorLabel->setText(tr("No mail servers found."));
    }

    updateOverlay();
}

void QnSmtpSettingsWidget::at_testButton_clicked() {

}

void QnSmtpSettingsWidget::at_autoDetectCheckBox_toggled(bool toggled) {
    ui->serverLineEdit->setEnabled(!toggled);
    ui->userLineEdit->setEnabled(!toggled);
    ui->portComboBox->setEnabled(!toggled);
    ui->tlsRadioButton->setEnabled(!toggled);
    ui->unsecuredRadioButton->setEnabled(!toggled);

    if (toggled && !m_autoMailServer.isEmpty()) {
        ui->serverLineEdit->setText(m_autoMailServer);
        ui->userLineEdit->setText(m_autoUser);
        ui->portComboBox->setCurrentIndex(0);
        ui->tlsRadioButton->setChecked(true);
        //TODO: #GDM setup other smtp settings
    }
}

void QnSmtpSettingsWidget::handleServers() {
    // Check the lookup succeeded.

}
