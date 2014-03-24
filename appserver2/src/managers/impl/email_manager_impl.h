#ifndef EMAIL_MANAGER_IMPL_H
#define EMAIL_MANAGER_IMPL_H

#include <api/model/kvpair.h>
#include <utils/common/email.h>
#include <api/model/email_attachment.h>
#include <smtpclient/QnSmtpMime>

namespace ec2
{
    class ApiEmailData;

    class EmailManagerImpl
    {
    public:
        EmailManagerImpl();

        void configure(const QnKvPairList& kvPairs);
        bool sendEmail(const ApiEmailData& message);

        bool testConnection(const QnEmail::Settings&);

    private:
        QnEmail::Settings m_settings;
        QString m_from;
    };
}

#endif // EMAIL_MANAGER_IMPL_H
