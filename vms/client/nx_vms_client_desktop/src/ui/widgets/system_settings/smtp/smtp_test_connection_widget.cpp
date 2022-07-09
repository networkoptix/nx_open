// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "smtp_test_connection_widget.h"
#include "ui_smtp_test_connection_widget.h"

#include <QtCore/QTimer>

#include <api/model/test_email_settings_reply.h>
#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <ui/dialogs/common/message_box.h>
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

QString QnSmtpTestConnectionWidget::errorString(
    const rest::RestResultWithData<QnTestEmailSettingsReply>& result) const
{
    using namespace nx::email;
    switch (result.data.errorCode)
    {
        case SmtpError::success:
            return tr("Success");
        case SmtpError::connectionTimeout:
        case SmtpError::responseTimeout:
        case SmtpError::sendDataTimeout:
            return tr("Connection timed out");
        case SmtpError::authenticationFailed:
            return tr("Authentication failed");
        default:
            // ServerError,    // 4xx SMTP error
            // ClientError     // 5xx SMTP error
            break;
    }
    return tr("SMTP Error %1").arg((int)result.data.smtpReplyCode);
}

bool QnSmtpTestConnectionWidget::testSettings(const QnEmailSettings& value)
{
    if (!value.isValid())
    {
        QnMessageBox::warning(this, tr("Invalid parameters"), tr("Cannot perform the test."));
        return false;
    }

    return performTesting(value);
}

bool QnSmtpTestConnectionWidget::testRemoteSettings()
{
    return performTesting({});
}

bool QnSmtpTestConnectionWidget::performTesting(QnEmailSettings settings)
{
    if (!NX_ASSERT(connection()))
        return false;

    settings.timeout = testSmtpTimeoutMSec / 1000;

    std::optional<QnUuid> proxyToServer;
    if (auto server = commonModule()->currentServer();
        NX_ASSERT(server) && !server->hasInternetAccess())
    {
        if (auto onlineServer = resourcePool()->serverWithInternetAccess())
            proxyToServer = onlineServer->getId();
    }

    ui->testServerLabel->setText(settings.server);
    ui->testPortLabel->setText(QString::number(settings.port == 0
        ? QnEmailSettings::defaultPort(settings.connectionType)
        : settings.port));
    ui->testUserLabel->setText(settings.user);
    ui->testSecurityLabel->setText(settings.connectionType == QnEmail::ConnectionType::tls
        ? tr("TLS")
        : settings.connectionType == QnEmail::ConnectionType::ssl
        ? tr("SSL")
        : tr("Unsecured"));

    ui->cancelTestButton->setVisible(true);
    ui->okTestButton->setVisible(false);

    ui->testProgressBar->setValue(0);

    ui->testResultLabel->setText(tr("In Progress..."));

    m_timeoutTimer->setInterval(testSmtpTimeoutMSec / ui->testProgressBar->maximum());
    m_timeoutTimer->start();

    auto callback = nx::utils::guarded(this,
        [this](bool success,
            int handle,
            const rest::RestResultWithData<QnTestEmailSettingsReply>& reply)
        {
            at_testEmailSettingsFinished(success, handle, reply);
        });

    m_testHandle = connectedServerApi()->testEmailSettings(
        settings,
        callback,
        thread(),
        proxyToServer);
    return m_testHandle > 0;
}

void QnSmtpTestConnectionWidget::at_testEmailSettingsFinished(
    bool success,
    int handle, const rest::RestResultWithData<QnTestEmailSettingsReply>& reply)
{
    if (handle != m_testHandle)
        return;

    QString result;
    if (success)
        result = errorString(reply);
    else
        result = tr("Network error");
    stopTesting(result);
}

