#include "appserver2_audit_manager.h"

#include <common/common_module.h>

namespace ec2 {

int AuditManager::addAuditRecord(const QnAuditRecord& /*record*/)
{
    return ++m_internalIdCounter;
}

int AuditManager::updateAuditRecord(int internalId, const QnAuditRecord& /*record*/)
{
    return internalId;
}

} // namespace ec2
