#include "mserver_audit_manager.h"

#include <api/common_message_processor.h>
#include <database/server_db.h>

#include "utils/common/synctime.h"
#include <nx/utils/log/assert.h>

QnAuditRecord filteredRecord(QnAuditRecord record)
{
    if (!record.isLoginType())
        record.authSession.userAgent.clear(); // optimization. It used for login type only
    return record;
}

QnMServerAuditManager::QnMServerAuditManager(
    std::chrono::milliseconds lastRunningTime, QObject* parent)
:
    QnAuditManager(parent),
    m_internalId(-1)
{
    const auto maxTime = std::chrono::duration_cast<std::chrono::seconds>(lastRunningTime);
    qnServerDb->closeUnclosedAuditRecords((int) maxTime.count());

    m_internalId = qnServerDb->auditRecordMaxId();
    connect(&m_timer, &QTimer::timeout, this, [this]() { flushRecords(); });
    m_timer.start(1000 * 5);
}

QnMServerAuditManager::~QnMServerAuditManager()
{
    m_timer.stop();
    flushRecords();
    qnServerDb->closeUnclosedAuditRecords((int) (qnSyncTime->currentMSecsSinceEpoch() / 1000));
}

int QnMServerAuditManager::addAuditRecordInternal(const QnAuditRecord& record)
{
    if (m_internalId < 0)
        return -1; //< error writing to server database

    if (record.isLoginType())
        NX_ASSERT(record.resources.empty());

    QnMutexLocker lock(&m_mutex);
    auto internalId = ++m_internalId;
    m_recordsToAdd[internalId] = filteredRecord(record);
    return internalId;
}

int QnMServerAuditManager::updateAuditRecordInternal(int internalId, const QnAuditRecord& record)
{
    if (m_internalId < 0)
        return -1; //< error writing to server database

    if (record.isLoginType())
        NX_ASSERT(record.resources.empty());

    QnMutexLocker lock(&m_mutex);
    m_recordsToAdd[internalId] = filteredRecord(record);
    return internalId;
}

void QnMServerAuditManager::flushRecords()
{
    decltype(m_recordsToAdd) records;
    {
        QnMutexLocker lock(&m_mutex);
        if (m_recordsToAdd.empty())
            return;

        std::swap(records, m_recordsToAdd);
    }

    if (!qnServerDb->addAuditRecords(records))
        qWarning() << "Failed to add" << records.size() << "audit trail records";
}
