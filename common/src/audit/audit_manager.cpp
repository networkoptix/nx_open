#include "audit_manager.h"
#include "utils/common/synctime.h"

static QnAuditManager* m_globalInstance = 0;

QnAuditManager* QnAuditManager::instance()
{
    return m_globalInstance;
}

QnAuditManager::QnAuditManager()
{
    assert( m_globalInstance == nullptr );
    m_globalInstance = this;
}

QnAuditRecord QnAuditManager::prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType)
{
    QnAuditRecord result;
    result.fillAuthInfo(authInfo);
    result.timestamp = qnSyncTime->currentMSecsSinceEpoch() / 1000;
    result.eventType = recordType;
    return result;
}

void QnAuditManager::at_connectionOpened(const QnAuthSession& authInfo)
{
    AuditConnection connection;
    connection.record = prepareRecord(authInfo, Qn::AR_Login);
    connection.internalId = addAuditRecord(connection.record);
    if (connection.internalId > 0) {
        QMutexLocker lock(&m_mutex);
        m_openedConnections[authInfo.sessionId] = connection;
    }
}

void QnAuditManager::at_connectionClosed(const QnAuthSession &data)
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_openedConnections.find(data.sessionId);
    if (itr != m_openedConnections.end()) {
        const AuditConnection& connection = itr.value();
        updateAuditRecord(connection.internalId, connection.record);
    }
    m_openedConnections.remove(data.sessionId);
}
