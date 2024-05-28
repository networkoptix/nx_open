// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AuditLogFilter, (json), AuditLogFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AuditRecord, (ubjson)(json), AuditRecord_Fields)

AuditRecord AuditRecord::prepareRecord(
    AuditRecordType type,
    const nx::network::rest::audit::Record& auditRecord,
    std::chrono::seconds createdTime)
{
    AuditRecord result(auditRecord);
    result.eventType = type;
    result.createdTimeS = createdTime;
    return result;
}

} // namespace nx::vms::api
