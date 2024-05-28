// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_manager.h"

#include <utils/common/synctime.h>

static std::optional<std::chrono::seconds> s_createdTimeForTests;

std::chrono::seconds QnAuditManager::createdTime()
{
    return s_createdTimeForTests.value_or(
        std::chrono::duration_cast<std::chrono::seconds>(qnSyncTime->currentTimePoint()));
}

void QnAuditManager::setCreatedTimeForTests(std::optional<std::chrono::seconds> value)
{
    s_createdTimeForTests = value;
}
