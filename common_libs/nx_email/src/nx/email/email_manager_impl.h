#ifndef EMAIL_MANAGER_IMPL_H
#define EMAIL_MANAGER_IMPL_H

#include <nx_ec/data/api_fwd.h>
#include <utils/email/email_fwd.h>

class EmailManagerImpl
{
public:
    EmailManagerImpl();

    SmtpOperationResult sendEmail(
        const QnEmailSettings& settings,
        const ec2::ApiEmailData& message) const;
    SmtpOperationResult sendEmail( const ec2::ApiEmailData& message ) const;
    SmtpOperationResult testConnection(const QnEmailSettings &settings) const;
};

#endif // EMAIL_MANAGER_IMPL_H
