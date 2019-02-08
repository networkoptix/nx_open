#include "audit_manager.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "api/global_settings.h"

#include <nx/utils/datetime.h>
#include <nx/utils/log/assert.h>
#include <common/common_module.h>

QnAuditRecord QnAuditManager::prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType)
{
    QnAuditRecord result;
    result.authSession = authInfo;
    result.createdTimeSec = qnSyncTime->currentMSecsSinceEpoch() / 1000;
    result.eventType = recordType;
    return result;
}
