#include "mserver_audit_manager.h"
#include "events/events_db.h"
#include "api/common_message_processor.h"
#include "utils/common/synctime.h"

namespace
{
    static const qint64 SESSION_CLEANUP_INTERVAL_MS = 1000;
    static const qint64 SESSION_KEEP_PERIOD_MS      = 1000 * 3600ll * 24;
}

QnAuditRecord filteredRecord(QnAuditRecord record)
{
    if (!record.isLoginType())
        record.authSession.userAgent.clear(); // optimization. It used for login type only
    return record;
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
    if (m_sessionCleanupTimer.elapsed() > SESSION_CLEANUP_INTERVAL_MS)
    {
        m_sessionCleanupTimer.restart();
        cleanupExpiredSessions();
    }

    if (record.eventType == Qn::AR_Login)
    {
        if (m_knownSessions.contains(record.authSession.id))
            return -1;
        m_knownSessions.insert(record.authSession.id, UINT_MAX * 1000ll);
    }
    else if (record.eventType == Qn::AR_UnauthorizedLogin) {
        ; // nothing to do
    }
    else if (!m_knownSessions.contains(record.authSession.id))
    {
        m_knownSessions.insert(record.authSession.id, record.createdTimeSec * 1000ll);
        QnAuditRecord loginRecord = prepareRecord(record.authSession, Qn::AR_Login);
        loginRecord.rangeStartSec = record.createdTimeSec;
        int insertedId = qnEventsDB->addAuditRecord(filteredRecord(std::move(loginRecord)));
        if (insertedId == -1)
            return insertedId; // error occured
    }
    else 
        m_knownSessions[record.authSession.id] = record.createdTimeSec * 1000ll; // update session time if some activity
    return qnEventsDB->addAuditRecord(filteredRecord(record));
}

int QnMServerAuditManager::updateAuditRecordInternal(int internalId, const QnAuditRecord& record)
{
    if (record.isLoginType())
        Q_ASSERT(record.resources.empty());

    QMutexLocker lock(&m_mutex);
    if (record.isLoginType() && record.rangeEndSec) {
		// it will be auto deleted later if fill session last activity time
        if (m_knownSessions.contains(record.authSession.id))
            m_knownSessions[record.authSession.id] = qnSyncTime->currentMSecsSinceEpoch();
    }

    return qnEventsDB->updateAuditRecord(internalId, filteredRecord(record));
}
