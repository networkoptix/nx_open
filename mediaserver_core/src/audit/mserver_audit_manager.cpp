#include "mserver_audit_manager.h"
#include "events/events_db.h"
#include "api/common_message_processor.h"


QnMServerAuditManager::QnMServerAuditManager(): QnAuditManager()
{
}

QnMServerAuditManager::~QnMServerAuditManager()
{
}

int QnMServerAuditManager::addAuditRecord(const QnAuditRecord& record)
{
    return qnEventsDB->addAuditRecord(record);
}

int QnMServerAuditManager::updateAuditRecord(int internalId, const QnAuditRecord& record)
{
    return qnEventsDB->updateAuditRecord(internalId, record);
}
