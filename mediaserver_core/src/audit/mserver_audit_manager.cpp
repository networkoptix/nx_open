#include "mserver_audit_manager.h"

#include <api/common_message_processor.h>
#include <database/server_db.h>

#include "utils/common/synctime.h"
#include <nx/utils/log/assert.h>

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
        NX_ASSERT(record.resources.empty());

    return qnServerDb->addAuditRecord(filteredRecord(record));
}

int QnMServerAuditManager::updateAuditRecordInternal(int internalId, const QnAuditRecord& record)
{
    if (record.isLoginType())
        NX_ASSERT(record.resources.empty());

    return qnServerDb->updateAuditRecord(internalId, filteredRecord(record));
}
