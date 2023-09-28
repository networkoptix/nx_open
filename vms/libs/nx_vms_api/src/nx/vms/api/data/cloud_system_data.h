// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API CloudSystemData
{
    QnUuid localSystemId;
};
#define CloudSystemData_Fields (localSystemId)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(CloudSystemData)
NX_REFLECTION_INSTRUMENT(CloudSystemData, CloudSystemData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
