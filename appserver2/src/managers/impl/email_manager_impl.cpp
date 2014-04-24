#include "email_manager_impl.h"

#include "nx_ec/data/api_email_data.h"
#include <memory>

namespace ec2
{
    EmailManagerImpl::EmailManagerImpl()
    {
    }

    void EmailManagerImpl::configure(const QnKvPairList& kvPairs) {
        m_settings = QnEmail::Settings(kvPairs);

        foreach(const QnKvPair& kvPair, kvPairs) {
            if (kvPair.name() == lit("EMAIL_FROM"))
                m_from = kvPair.value();
        }
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

        MimeHtml text(data.body);
        message.addPart(&text);

        // Need to store all attachements as smtp client operates on attachment pointers
        typedef std::shared_ptr<MimeInlineFile> MimeInlineFilePtr; 
        std::vector<MimeInlineFilePtr> attachments;
        foreach (QnEmailAttachmentPtr attachment, data.attachments) {
            MimeInlineFilePtr att(new MimeInlineFile(attachment->content, attachment->filename, attachment->mimetype));
            attachments.push_back(att);
            message.addPart(att.get());
        }

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