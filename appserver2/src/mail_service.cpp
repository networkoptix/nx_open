#include "mail_service.h"

QnMailService::~QnMailService() {
    stop();
}

void QnMailService::configure(const QnMailService::Settings& settings) {
    m_settings = settings;
}

void QnMailService::run() {
    while (needToStop()) {
        try {
            MimeMessagePtr message = m_queue.wait();
            doSend(*message);
        } catch (const QueueType::Empty&) {
            continue;
        }
    }
}

void QnMailService::sendEmail(const QStringList& to, const QString& subject, const QString& body, int timeout, const QnEmailAttachmentList& attachments) {
    MimeMessagePtr message(new MimeMessage());

    message->setSender(new EmailAddress(m_settings.from));
    foreach (const QString &recipient, to) {
        message->addRecipient(new EmailAddress(recipient));
    }
    
    message->setSubject(subject);

    MimeText text(body);
    message->addPart(&text);

    foreach (QnEmailAttachmentPtr attachment, attachments) {
        MimeAttachment mat(attachment->content, attachment->filename);
        message->addPart(&mat);
    }

    m_queue.push(message);
}

void QnMailService::doSend(MimeMessage& message) {
    SmtpClient smtp(m_settings.host, m_settings.port, SmtpClient::SslConnection);

    smtp.setUser(m_settings.user);
    smtp.setPassword(m_settings.password);

    smtp.connectToHost();
    smtp.login();
    smtp.sendMail(message);
    smtp.quit();
}