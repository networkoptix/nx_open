#include "email_manager_impl.h"

#include <memory>

#include <api/global_settings.h>
#include <api/model/email_attachment.h>

#include <smtpclient/smtpclient.h>
#include <smtpclient/QnSmtpMime>

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
    QString sender = QString(lit("%1@%2")).arg(settings.user).arg(settings.server);
    message.setSender(new EmailAddress(sender));
    foreach (const QString &recipient, data.to) {
        message.addRecipient(new EmailAddress(recipient));
    }
    
    message.setSubject(data.subject);

    MimeHtml text(data.body);
    message.addPart(&text);

    // Need to store all attachments as smtp client operates on attachment pointers
    typedef std::shared_ptr<MimeInlineFile> MimeInlineFilePtr; 
    std::vector<MimeInlineFilePtr> attachments;
    foreach (QnEmailAttachmentPtr attachment, data.attachments) {
        MimeInlineFilePtr att(new MimeInlineFile(attachment->content, attachment->filename, attachment->mimetype));
        attachments.push_back(att);
        message.addPart(att.get());
    }

    // Actually send
    int port = settings.port ? settings.port : QnEmail::defaultPort(settings.connectionType);
    SmtpClient::ConnectionType connectionType = smtpConnectionType(settings.connectionType);
    SmtpClient smtp(settings.server, port, connectionType);

    smtp.setUser(settings.user);
    smtp.setPassword(settings.password);

    if (!smtp.connectToHost()) return false;
    bool result = smtp.login();
    if (result)
        result = smtp.sendMail(message);

    smtp.quit();

    return result;
}