#ifndef EMAIL_MANAGER_IMPL_H
#define EMAIL_MANAGER_IMPL_H

#include <nx_ec/data/api_fwd.h>
#include <utils/common/email.h>

namespace ec2
{
    class EmailManagerImpl
    {
    public:
        EmailManagerImpl();

        bool sendEmail(const ApiEmailData& message);
        bool testConnection(const QnEmail::Settings &settings);

    };
}

#endif // EMAIL_MANAGER_IMPL_H
