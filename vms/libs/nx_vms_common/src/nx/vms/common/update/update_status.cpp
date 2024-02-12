// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_status.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::common::update {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Status, (json), UpdateStatus_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PackageDownloadStatus, (json), PackageDownloadStatus_Fields)

Status::Status(
    const nx::Uuid& serverId,
    Status::Code code,
    Status::ErrorCode errorCode,
    int progress)
    :
    serverId(serverId),
    code(code),
    errorCode(errorCode),
    progress(progress)
{
}

bool Status::suitableForRetrying() const
{
    return code == Code::error || code == Code::idle;
}

uint qHash(Status::ErrorCode key, uint seed)
{
    return ::qHash(static_cast<uint>(key), seed);
}

} // namespace nx::vms::common::update
