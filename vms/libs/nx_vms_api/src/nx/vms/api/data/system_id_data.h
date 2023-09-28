// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

#include "data_macros.h"
#include "timestamp.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API SystemIdData
{
    SystemIdData() = default;
    SystemIdData(const QString& systemId): systemId(systemId) {}

    QnUuid systemId;
    qint64 sysIdTime = 0;
    Timestamp tranLogTime;
};
#define SystemIdData_Fields (systemId)(sysIdTime)(tranLogTime)
NX_VMS_API_DECLARE_STRUCT(SystemIdData)

} // namespace api
} // namespace vms
} // namespace nx
