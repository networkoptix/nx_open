#include "email_manager_impl.h"

#include <memory>

#include <api/global_settings.h>
#include <api/model/email_attachment.h>
#include <nx_ec/data/api_email_data.h>
#include <nx/utils/log/log.h>
#include <utils/email/email.h>

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

bool EmailManagerImpl::testConnection(const QnEmailSettings &settings) const
{
    int port = settings.port ? settings.port : QnEmailSettings::defaultPort(settings.connectionType);

    SmtpClient::ConnectionType connectionType = smtpConnectionType(settings.connectionType);
    SmtpClient smtp(settings.server, port, connectionType);

    if (!smtp.connectToHost())
        return false;

    bool result = settings.user.isEmpty() || smtp.login(settings.user, settings.password);
    smtp.quit();

    return result;
}

bool EmailManagerImpl::sendEmail(
    const QnEmailSettings& settings,
    const ec2::ApiEmailData& data ) const
{
    if (!settings.isValid())
        return true;    // empty settings should not give us an error while trying to send email, should them?

    MimeMessage message;
    QString sender;
    if (!settings.email.isEmpty())
        sender = settings.email;
    else if (settings.user.contains(L'@'))
        sender = settings.user;
    else if (settings.server.startsWith(lit("smtp.")))
        sender = QString(lit("%1@%2")).arg(settings.user).arg(settings.server.mid(5));
    else
        sender = QString(lit("%1@%2")).arg(settings.user).arg(settings.server);

    message.setSender(EmailAddress(sender));
    for (const QString &recipient: data.to) {
        message.addRecipient(EmailAddress(recipient));
    }

    message.setSubject(data.subject);
    message.addPart(new MimeHtml(data.body));

    // Need to store all attachments as smtp client operates on attachment pointers
    for (const QnEmailAttachmentPtr& attachment: data.attachments)
        message.addPart(new MimeInlineFile(attachment->content, attachment->filename, attachment->mimetype));

    // Actually send
    int port = settings.port ? settings.port : QnEmailSettings::defaultPort(settings.connectionType);
    SmtpClient::ConnectionType connectionType = smtpConnectionType(settings.connectionType);
    SmtpClient smtp(settings.server, port, connectionType);

    if( !smtp.connectToHost() )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit("SMTP. Failed to connect to %1:%2 with %3. %4").arg(settings.server).arg(port)
            .arg(SmtpClient::toString(connectionType)).arg(SystemError::toString(errorCode)), cl_logWARNING );
        return false;
    }

    if (!settings.user.isEmpty() && !smtp.login(settings.user, settings.password) )
    {
        NX_LOG( lit("SMTP. Failed to login to %1:%2").arg(settings.server).arg(port), cl_logWARNING );
        smtp.quit();
        return false;
    }

    if( !smtp.sendMail(message) )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit("SMTP. Failed to send mail to %1:%2. %3").arg(settings.server).arg(port).arg(SystemError::toString(errorCode)), cl_logWARNING );
        smtp.quit();
        return false;
    }
    smtp.quit();

    return true;
}

bool EmailManagerImpl::sendEmail( const ec2::ApiEmailData& data ) const
{
    return sendEmail(
        QnGlobalSettings::instance()->emailSettings(),
        data );
}
