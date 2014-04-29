#ifndef EMAIL_MANAGER_IMPL_H
#define EMAIL_MANAGER_IMPL_H

#include <nx_ec/data/api_fwd.h>
#include <api/model/kvpair.h>
#include <utils/common/email.h>
#include <api/model/email_attachment.h>
#include <smtpclient/QnSmtpMime>

namespace ec2
{
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
