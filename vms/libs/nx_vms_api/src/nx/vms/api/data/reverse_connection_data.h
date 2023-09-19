// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

/** Request to open count proxy connections to @var target server. */
struct NX_VMS_API ReverseConnectionData
{
    QnUuid targetServer;
    int socketCount = 0;
};
#define ReverseConnectionData_Fields (targetServer)(socketCount)
NX_VMS_API_DECLARE_STRUCT(ReverseConnectionData)

} // namespace api
} // namespace vms
} // namespace nx
