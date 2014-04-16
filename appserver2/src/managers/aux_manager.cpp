
#include "aux_manager.h"

#include "version.h"

#include "common/common_module.h"
#include "managers/impl/email_manager_impl.h"


namespace ec2
{

static QnAuxManager* globalInstance = 0;


QnAuxManager::QnAuxManager(EmailManagerImpl* const emailManagerImpl)
    : m_emailManagerImpl(emailManagerImpl)
{
    Q_ASSERT(!globalInstance);
    globalInstance = this;
}

QnAuxManager::~QnAuxManager()
{
    globalInstance = 0;
}

QnAuxManager* QnAuxManager::instance()
{
    return globalInstance;
}

ErrorCode QnAuxManager::executeTransaction(const QnTransaction<ApiEmailSettingsData>& tran)
{
    QnEmail::Settings settings;
    tran.params.toResource(settings);
    return m_emailManagerImpl->testConnection(settings) ? ErrorCode::ok : ErrorCode::failure;
}

ErrorCode QnAuxManager::executeTransaction(const QnTransaction<ApiEmailData>& tran)
{
    return m_emailManagerImpl->sendEmail(tran.params) ? ErrorCode::ok : ErrorCode::failure;
}
} // namespace ec2
