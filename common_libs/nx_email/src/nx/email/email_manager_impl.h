#ifndef EMAIL_MANAGER_IMPL_H
#define EMAIL_MANAGER_IMPL_H

#include <nx_ec/data/api_fwd.h>
#include <utils/email/email_fwd.h>
#include <common/common_module_aware.h>

class EmailManagerImpl: public QnCommonModuleAware
{
public:
    EmailManagerImpl(QnCommonModule* commonModule);

    SmtpOperationResult sendEmail(
        const QnEmailSettings& settings,
        const ec2::ApiEmailData& message) const;
    SmtpOperationResult sendEmail( const ec2::ApiEmailData& message ) const;
    SmtpOperationResult testConnection(const QnEmailSettings &settings) const;
};

#endif // EMAIL_MANAGER_IMPL_H
