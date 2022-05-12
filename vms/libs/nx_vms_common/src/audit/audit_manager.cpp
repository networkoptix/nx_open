// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_manager.h"

#include <common/common_module.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

QnAuditRecord QnAuditManager::prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType)
{
    QnAuditRecord result;
    result.authSession = authInfo;
    result.createdTimeSec = qnSyncTime->currentMSecsSinceEpoch() / 1000;
    result.eventType = recordType;
    return result;
}
