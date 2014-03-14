#ifndef OPTIX_COMMON_MAIL_SERVICE_H
#define OPTIX_COMMON_MAIL_SERVICE_H

#include <QtCore/QThread>
#include <QtCore/QQueue>

#include <utils/common/async_queue.h>
#include <utils/common/long_runnable.h>
#include <api/model/email_attachment.h>

#include <smtpclient/QnSmtpMime>

class QnMailService : public QnLongRunnable {
    Q_OBJECT
public:
    ~QnMailService();

    struct Settings {
        QString from;
        QString host;
        int port;
        QString user;
        QString password;
        bool useTls;
        bool useSsl;
    };

    enum SendStatus {
        SUCCESS,
        FAIL
    };

    void configure(const Settings&);
    void sendEmail(const QStringList& to, const QString& subject, const QString& message, int timeout, const QnEmailAttachmentList&);

    void run();

signals:
    void emailSendResult(SendStatus status, QString errorString); 

private:
    void doSend(MimeMessage& message);

private:
    Settings m_settings;
    typedef QnAsyncQueue<MimeMessagePtr> QueueType;
    QueueType m_queue;
};

#endif // OPTIX_COMMON_MAIL_SERVICE_H