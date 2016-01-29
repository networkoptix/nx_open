#include "smtp_test_connection_widget.h"
#include "ui_smtp_test_connection_widget.h"

#include <api/model/test_email_settings_reply.h>
#include <api/media_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <utils/email/email.h>

namespace {
    const int testSmtpTimeoutMSec = 20 * 1000;
}

QnSmtpTestConnectionWidget::QnSmtpTestConnectionWidget( QWidget* parent /*= nullptr*/ )
    : QWidget(parent)
    , ui(new Ui::SmtpTestConnectionWidget)
    , m_timeoutTimer(new QTimer(this))
    , m_testHandle(0)
{
    ui->setupUi(this);

    m_timeoutTimer->setSingleShot(false);

    connect(ui->cancelTestButton,           &QPushButton::clicked,              this,   [this] {
        stopTesting();
        emit finished();
    });
    connect(ui->okTestButton,               &QPushButton::clicked,              this,   &QnSmtpTestConnectionWidget::finished);

    connect(m_timeoutTimer,                 &QTimer::timeout,                   this,   &QnSmtpTestConnectionWidget::at_timer_timeout);
}

QnSmtpTestConnectionWidget::~QnSmtpTestConnectionWidget()
{}

void QnSmtpTestConnectionWidget::stopTesting( const QString &result /*= QString()*/ ) {
    if (m_testHandle < 0)
        return;
    m_timeoutTimer->stop();
    m_testHandle = -1;

    ui->testResultLabel->setText(result);
    ui->testProgressBar->setValue(ui->testProgressBar->maximum());
    ui->cancelTestButton->setVisible(false);
    ui->okTestButton->setVisible(true);
}

void QnSmtpTestConnectionWidget::at_timer_timeout() {
    int value = ui->testProgressBar->value();
    if (value < ui->testProgressBar->maximum()) {
        ui->testProgressBar->setValue(value + 1);
        return;
    }

    stopTesting(tr("Timed Out"));
}

bool QnSmtpTestConnectionWidget::testSettings( const QnEmailSettings &value ) {

    QnEmailSettings result = value;
    result.timeout = testSmtpTimeoutMSec / 1000;

    if (!result.isValid()) {
        QMessageBox::warning(this, tr("Invalid data"), tr("The provided parameters are not valid. Could not perform a test."));
        return false;
    }

    QnMediaServerConnectionPtr serverConnection;
    const auto onlineServers = qnResPool->getAllServers(Qn::Online);
    for(const QnMediaServerResourcePtr &server: onlineServers)
    {
        if (!server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
            continue;

        serverConnection = server->apiConnection();
        break;
    }

    if (!serverConnection) {
        QMessageBox::warning(this, tr("Network Error"), tr("Could not perform a test. None of your servers are connected to the Internet."));
        return false;
    }

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

    m_testHandle = serverConnection->testEmailSettingsAsync(result, this, SLOT(at_testEmailSettingsFinished(int, const QnTestEmailSettingsReply& , int)));
    return m_testHandle > 0;
}

void QnSmtpTestConnectionWidget::at_testEmailSettingsFinished( int status, const QnTestEmailSettingsReply& reply, int handle ) {
    if (handle != m_testHandle)
        return;

    stopTesting(status != 0 || reply.errorCode != 0
        ? tr("Failed")
        : tr("Success") );
}

