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


namespace {
    SmtpClient::ConnectionType smtpConnectionType(QnEmail::ConnectionType ct) {
        if (ct == QnEmail::Ssl)
            return SmtpClient::SslConnection;
        else if (ct == QnEmail::Tls)
            return SmtpClient::TlsConnection;

        return SmtpClient::TcpConnection;
    }
}


EmailManagerImpl::EmailManagerImpl()
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
    else if (settings.server.startsWith(lit("smtp.")))
        sender = QString(lit("%1@%2")).arg(settings.user).arg(settings.server.mid(5));
    else
        sender = QString(lit("%1@%2")).arg(settings.user).arg(settings.server);

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
        NX_LOG( lit("SMTP. Failed to connect to %1:%2 with %3. %4 Error: %5")
            .arg(settings.server)
            .arg(port)
            .arg(SmtpClient::toString(connectionType))
            .arg(SystemError::toString(errorCode))
            .arg(lastSmtpResult.toString())
            , cl_logWARNING );
        return lastSmtpResult;
    }

    if (!settings.user.isEmpty())
    {
        lastSmtpResult = smtp.login(settings.user, settings.password);
        if (!lastSmtpResult)
        {
            NX_LOG( lit("SMTP. Failed to login to %1:%2 Error: %3")
                .arg(settings.server)
                .arg(port)
                .arg(lastSmtpResult.toString())
                , cl_logWARNING );
            smtp.quit();
            return lastSmtpResult;
        }
    }

    lastSmtpResult = smtp.sendMail(message);
    if (!lastSmtpResult)
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit("SMTP. Failed to send mail to %1:%2. %3 Error %4")
            .arg(settings.server)
            .arg(port)
            .arg(SystemError::toString(errorCode))
            .arg(lastSmtpResult.toString())
            , cl_logWARNING );
        smtp.quit();
        return lastSmtpResult;
    }
    smtp.quit();

    return lastSmtpResult;
}

SmtpOperationResult EmailManagerImpl::sendEmail( const ec2::ApiEmailData& data ) const
{
    return sendEmail(
        QnGlobalSettings::instance()->emailSettings(),
        data );
}
