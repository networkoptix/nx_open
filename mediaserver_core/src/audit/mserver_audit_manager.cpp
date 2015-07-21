#include "mserver_audit_manager.h"
#include "events/events_db.h"


QnMServerAuditManager::QnMServerAuditManager(): QnAuditManager()
{
}

void QnMServerAuditManager::addAuditRecord(const QnAuditRecord& record)
{
    if (qnEventsDB)
        qnEventsDB->addAuditRecord(record);
}
