#ifndef EMAIL_MANAGER_IMPL_H
#define EMAIL_MANAGER_IMPL_H

#include <nx_ec/data/api_fwd.h>
#include <utils/common/email.h>

class EmailManagerImpl
{
public:
    EmailManagerImpl();

    bool sendEmail(const ec2::ApiEmailData& message);
    bool testConnection(const QnEmail::Settings &settings);

};

#endif // EMAIL_MANAGER_IMPL_H
