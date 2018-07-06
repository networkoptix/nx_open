#include "appserver2_audit_manager.h"

#include <common/common_module.h>

namespace ec2 {

AuditManager::AuditManager(QnCommonModule* commonModule):
    base_type(commonModule),
    m_internalIdCounter(0)
{
}

int AuditManager::addAuditRecordInternal(const QnAuditRecord& /*record*/)
{
    return ++m_internalIdCounter;
}

int AuditManager::updateAuditRecordInternal(int internalId, const QnAuditRecord& /*record*/)
{
    return internalId;
}

} // namespace ec2
