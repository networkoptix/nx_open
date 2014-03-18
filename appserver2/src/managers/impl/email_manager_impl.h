#ifndef EMAIL_MANAGER_IMPL_H
#define EMAIL_MANAGER_IMPL_H

#include <utils/common/email.h>
#include <api/model/email_attachment.h>
#include <smtpclient/QnSmtpMime>

namespace ec2
{
    class EmailManagerImpl
    {
    public:
        EmailManagerImpl();

        void configure(const QnEmail::Settings&, const QString&);
        bool sendEmail(const QStringList& to, const QString& subject, const QString& message, int timeout, const QnEmailAttachmentList&);

        bool testConnection(const QnEmail::Settings&);

    private:
        QnEmail::Settings m_settings;
        QString m_from;
    };
}

#endif // EMAIL_MANAGER_IMPL_H
