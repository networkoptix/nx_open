// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>

#include <core/resource_access/resource_access_map.h>
#include <core/resource_access/resource_access_details.h>
#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::api {

void PrintTo(AccessRights accessRights, std::ostream* os);

} // namespace nx::vms::api

void PrintTo(const nx::core::access::ResourceAccessMap& map, std::ostream* os);
void PrintTo(const nx::core::access::ResourceAccessDetails& details, std::ostream* os);
