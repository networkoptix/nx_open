#include "email_manager_impl.h"

#include "nx_ec/data/ec2_email.h"

namespace ec2
{
    EmailManagerImpl::EmailManagerImpl()
    {
    }

    void EmailManagerImpl::configure(const QnEmail::Settings &settings, const QString &from) {
        m_settings = settings;
        m_from = from;
    }

    SmtpClient::ConnectionType smtpConnectionType(QnEmail::ConnectionType ct) {
        if (ct == QnEmail::Ssl)
            return SmtpClient::SslConnection;
        else if (ct == QnEmail::Tls)
            return SmtpClient::TlsConnection;

        return SmtpClient::TcpConnection;
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
        MimeMessage message;
        message.setSender(new EmailAddress(m_from));
        foreach (const QString &recipient, data.to) {
            message.addRecipient(new EmailAddress(recipient));
        }
    
        message.setSubject(data.subject);

        MimeText text(data.body);
        message.addPart(&text);

/*        foreach (QnEmailAttachmentPtr attachment, data.attachments) {
            MimeAttachment mat(attachment->content, attachment->filename);
            message.addPart(&mat);
        } */

        // Actually send
        int port = m_settings.port ? m_settings.port : QnEmail::defaultPort(m_settings.connectionType);
        SmtpClient::ConnectionType connectionType = smtpConnectionType(m_settings.connectionType);
        SmtpClient smtp(m_settings.server, port, connectionType);

        smtp.setUser(m_settings.user);
        smtp.setPassword(m_settings.password);

        if (!smtp.connectToHost()) return false;
        bool result = smtp.login();
        if (result)
            result = smtp.sendMail(message);

        smtp.quit();

        return result;
    }
}