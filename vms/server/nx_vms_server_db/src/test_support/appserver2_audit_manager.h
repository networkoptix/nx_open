#pragma once

#include <atomic>

#include <audit/audit_manager.h>

class QnCommonModule;

namespace ec2 {

class AuditManager: public QnAuditManager
{
    using base_type = QnAuditManager;

public:
    virtual AuditHandle notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& id, qint64 timestampUsec, bool isExport = false) override
    {
        return AuditHandle();
    }
    virtual void notifyPlaybackInProgress(const AuditHandle& handle, qint64 timestampUsec) {}
    virtual void notifySettingsChanged(const QnAuthSession& authInfo, const QString& paramName) {}

    /* return internal id of inserted record. Returns <= 0 if error */
    virtual int addAuditRecord(const QnAuditRecord& record) override;
    virtual int updateAuditRecord(int internalId, const QnAuditRecord& record) override;
    virtual QnTimePeriod playbackRange(const AuditHandle& handle) const override { return QnTimePeriod(); }

    virtual void at_connectionOpened(const QnAuthSession& session) override {}
    virtual void at_connectionClosed(const QnAuthSession& session) override {}

private:
    std::atomic<int> m_internalIdCounter{0};
};

} // namespace ec2
