#include "email_manager_impl.h"

#include <memory>

#include <api/global_settings.h>
#include <api/model/email_attachment.h>

#include <smtpclient/smtpclient.h>
#include <smtpclient/QnSmtpMime>
#include <utils/common/log.h>

#include "nx_ec/data/api_email_data.h"

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

bool EmailManagerImpl::testConnection(const QnEmail::Settings &settings) {
    int port = settings.port ? settings.port : QnEmail::defaultPort(settings.connectionType);

    SmtpClient::ConnectionType connectionType = smtpConnectionType(settings.connectionType);
    SmtpClient smtp(settings.server, port, connectionType);

    smtp.setUser(settings.user);
    smtp.setPassword(settings.password);

    if (!smtp.connectToHost()) return false;
    bool result = smtp.login();
    smtp.quit();
    return result;
}

bool EmailManagerImpl::sendEmail(const ec2::ApiEmailData& data) {

    QnEmail::Settings settings = QnGlobalSettings::instance()->emailSettings();
    if (!settings.isValid())
        return true;    // empty settings should not give us an error while trying to send email, should them?

    MimeMessage message;
    QString sender;
    if (settings.user.contains(L'@'))
        sender = settings.user;
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
    int port = settings.port ? settings.port : QnEmail::defaultPort(settings.connectionType);
    SmtpClient::ConnectionType connectionType = smtpConnectionType(settings.connectionType);
    SmtpClient smtp(settings.server, port, connectionType);

    smtp.setUser(settings.user);
    smtp.setPassword(settings.password);

    if( !smtp.connectToHost() )
    {
        const SystemError::ErrorCode errorCode = SystemError::getLastOSErrorCode();
        NX_LOG( lit("SMTP. Failed to connect to %1:%2 with %3. %4").arg(settings.server).arg(port)
            .arg(SmtpClient::toString(connectionType)).arg(SystemError::toString(errorCode)), cl_logWARNING );
        return false;
    }
    if( !smtp.login() )
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
