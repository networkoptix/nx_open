#include "mserver_audit_manager.h"
#include "events/events_db.h"
#include "api/common_message_processor.h"
#include "utils/common/synctime.h"

namespace
{
}

QnAuditRecord filteredRecord(QnAuditRecord record)
{
    if (!record.isLoginType())
        record.authSession.userAgent.clear(); // optimization. It used for login type only
    return record;
}


QnMServerAuditManager::QnMServerAuditManager(): QnAuditManager()
{
}

QnMServerAuditManager::~QnMServerAuditManager()
{
}

int QnMServerAuditManager::addAuditRecordInternal(const QnAuditRecord& record)
{
    if (record.isLoginType())
        Q_ASSERT(record.resources.empty());
    return qnEventsDB->addAuditRecord(filteredRecord(record));
}

int QnMServerAuditManager::updateAuditRecordInternal(int internalId, const QnAuditRecord& record)
{
    if (record.isLoginType())
        Q_ASSERT(record.resources.empty());

    return qnEventsDB->updateAuditRecord(internalId, filteredRecord(record));
}
