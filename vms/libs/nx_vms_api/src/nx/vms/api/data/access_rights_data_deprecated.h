// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API AccessRightsDataDeprecated
{
    nx::Uuid userId;
    std::vector<nx::Uuid> resourceIds;
};
#define AccessRightsDataDeprecated_Fields (userId)(resourceIds)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(AccessRightsDataDeprecated, (ubjson)(json))

} // namespace nx::vms::api
