#include "mserver_audit_manager.h"
#include "events/events_db.h"
#include "api/common_message_processor.h"
#include "utils/common/synctime.h"

namespace
{
    static const int SESSION_CLEANUP_INTERVAL_MS = 1000;
    static const int SESSION_KEEP_PERIOD_MS = 1000;
}


QnMServerAuditManager::QnMServerAuditManager(): QnAuditManager()
{
    m_sessionCleanupTimer.restart();
}

QnMServerAuditManager::~QnMServerAuditManager()
{
}

void QnMServerAuditManager::cleanupExpiredSessions()
{
    qint64 currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    for (auto itr = m_knownSessions.begin(); itr != m_knownSessions.end();)
    {
        if (currentTimeMs - itr.value() > SESSION_KEEP_PERIOD_MS)
            itr = m_knownSessions.erase(itr);
        else
            ++itr;
    }
};

int QnMServerAuditManager::addAuditRecordInternal(const QnAuditRecord& record)
{
    if (record.isLoginType())
        Q_ASSERT(record.resources.empty());

    QMutexLocker lock(&m_mutex);
    if (m_sessionCleanupTimer.elapsed() >> SESSION_CLEANUP_INTERVAL_MS)
    {
        m_sessionCleanupTimer.restart();
        cleanupExpiredSessions();
    }

    if (record.eventType == Qn::AR_Login)
    {
        m_knownSessions.insert(record.sessionId, INT64_MAX);
    }
    else if (record.eventType == Qn::AR_UnauthorizedLogin) {
        ; // nothing to do
    }
    else if (!m_knownSessions.contains(record.sessionId))
    {
        m_knownSessions.insert(record.sessionId, record.createdTimeSec * 1000ll);
        QnAuditRecord loginRecord = record;
        loginRecord.eventType = Qn::AR_Login;
        loginRecord.rangeStartSec = record.createdTimeSec;
        int insertedId = qnEventsDB->addAuditRecord(loginRecord);
        if (insertedId == -1)
            return insertedId; // error occured
    }
    else 
        m_knownSessions[record.sessionId] = record.createdTimeSec * 1000ll; // update session time if some activity
    return qnEventsDB->addAuditRecord(record);
}

int QnMServerAuditManager::updateAuditRecordInternal(int internalId, const QnAuditRecord& record)
{
    if (record.isLoginType())
        Q_ASSERT(record.resources.empty());

    QMutexLocker lock(&m_mutex);
    if (record.isLoginType() && record.rangeEndSec)
        m_knownSessions.remove(record.sessionId);

    return qnEventsDB->updateAuditRecord(internalId, record);
}
