#pragma once

#include <atomic>

#include <audit/audit_manager.h>

class QnCommonModule;

namespace ec2 {

class AuditManager:
    public QnAuditManager
{
    using base_type = QnAuditManager;

public:
    AuditManager(QnCommonModule* commonModule);

protected:
    virtual int addAuditRecordInternal(const QnAuditRecord& record) override;
    virtual int updateAuditRecordInternal(int internalId, const QnAuditRecord& record) override;

private:
    std::atomic<int> m_internalIdCounter;
};

} // namespace ec2
