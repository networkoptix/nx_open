#include "mserver_audit_manager.h"
#include "events/events_db.h"
#include "api/common_message_processor.h"


QnMServerAuditManager::QnMServerAuditManager(): QnAuditManager()
{
}

QnMServerAuditManager::~QnMServerAuditManager()
{
}

void QnMServerAuditManager::addAuditRecord(const QnAuditRecord& record)
{
    qnEventsDB->addAuditRecord(record);
}

void QnMServerAuditManager::at_connectionOpened(const QnAuthSession &data)
{
    QnAuditRecord record;
    //record.fillAuthInfo(authInfo);
    //record.timestamp = qnSyncTime->currentMSecsSinceEpoch() / 1000;
    //record.eventType = recordType;
}
