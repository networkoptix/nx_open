#include "email_manager_impl.h"

#include <memory>

#include <api/global_settings.h>
#include <api/model/email_attachment.h>
#include <nx_ec/data/api_email_data.h>
#include <nx/utils/log/log.h>
#include <utils/email/email.h>
#include <utils/crypt/symmetrical.h>

#include "smtpclient/smtpclient.h"
#include "smtpclient/QnSmtpMime"
#include <common/common_module.h>


namespace {
    SmtpClient::ConnectionType smtpConnectionType(QnEmail::ConnectionType ct)
    {
        if (ct == QnEmail::ConnectionType::ssl)
            return SmtpClient::SslConnection;
        else if (ct == QnEmail::ConnectionType::tls)
            return SmtpClient::TlsConnection;

        return SmtpClient::TcpConnection;
    }
}

EmailManagerImpl::EmailManagerImpl(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

SmtpOperationResult EmailManagerImpl::testConnection(const QnEmailSettings &settings) const
{
    int port = settings.port ? settings.port : QnEmailSettings::defaultPort(settings.connectionType);

    SmtpClient::ConnectionType connectionType = smtpConnectionType(settings.connectionType);
    SmtpClient smtp(settings.server, port, connectionType);

    SmtpOperationResult lastSmtpResult;

    lastSmtpResult = smtp.connectToHost();
    if (!lastSmtpResult)
        return lastSmtpResult;

    if (!settings.user.isEmpty())
        lastSmtpResult = smtp.login(settings.user, settings.password);

    smtp.quit();

    return lastSmtpResult;
}

SmtpOperationResult EmailManagerImpl::sendEmail(
    const QnEmailSettings& settings,
    const ec2::ApiEmailData& data ) const
{
    if (!settings.isValid())
        return SmtpOperationResult();    // empty settings should not give us an error while trying to send email, should them?

    MimeMessage message;
    QString sender;
    if (!settings.email.isEmpty())
        sender = QnEmailAddress(settings.email).value();
    else if (settings.user.contains(L'@'))
        sender = settings.user;
    else if (settings.server.startsWith("smtp."))
        sender = QString("%1@%2").arg(settings.user, settings.server.mid(5));
    else
        sender = QString("%1@%2").arg(settings.user, settings.server);

    message.setSender(EmailAddress(sender));
    for (const QString &recipient: data.to)
        message.addRecipient(EmailAddress(recipient));

    message.setSubject(data.subject);

    MimeMultiPart *mainPart = new MimeMultiPart(MimeMultiPart::Alternative);
    MimeMultiPart *bodyPart = new MimeMultiPart(MimeMultiPart::Related);
    if (!data.body.isEmpty())
    {
        mainPart->addPart(new MimeText(data.plainBody));
        bodyPart->addPart(new MimeHtml(data.body));

        for (const QnEmailAttachmentPtr& attachment: data.attachments)
            bodyPart->addPart(new MimeInlineFile(attachment->content, attachment->filename, attachment->mimetype));
    } else
    {
        bodyPart->addPart(new MimeText(data.plainBody));

        for (const QnEmailAttachmentPtr& attachment: data.attachments)
            bodyPart->addPart(new MimeFile(attachment->content, attachment->filename, attachment->mimetype));
    }

    mainPart->addPart(bodyPart);

    message.setContent(mainPart);

    // Actually send
    int port = settings.port ? settings.port : QnEmailSettings::defaultPort(settings.connectionType);
    SmtpClient::ConnectionType connectionType = smtpConnectionType(settings.connectionType);
    SmtpClient smtp(settings.server, port, connectionType);

    SmtpOperationResult lastSmtpResult;
    lastSmtpResult = smtp.connectToHost();

    if (!lastSmtpResult)
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_WARNING(this, lm("Failed to connect to %1:%2 with %3. %4 Error: %5").args(
            settings.server,
            port,
            SmtpClient::toString(connectionType),
            SystemError::toString(errorCode),
            lastSmtpResult.toString()));
        return lastSmtpResult;
    }

    if (!settings.user.isEmpty())
    {
        lastSmtpResult = smtp.login(settings.user, settings.password);
        if (!lastSmtpResult)
        {
            NX_WARNING(this, lm("Failed to login to %1:%2 Error: %3").args(
                settings.server,
                port,
                lastSmtpResult.toString()));
            smtp.quit();
            return lastSmtpResult;
        }
    }

    lastSmtpResult = smtp.sendMail(message);
    if (!lastSmtpResult)
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_WARNING(this, lm("Failed to send mail to %1:%2. %3 Error %4").args(
            settings.server,
            port,
            SystemError::toString(errorCode),
            lastSmtpResult.toString()));
        smtp.quit();
        return lastSmtpResult;
    }
    smtp.quit();

    return lastSmtpResult;
}

SmtpOperationResult EmailManagerImpl::sendEmail( const ec2::ApiEmailData& data ) const
{
    return sendEmail(
        commonModule()->globalSettings()->emailSettings(),
        data );
}
