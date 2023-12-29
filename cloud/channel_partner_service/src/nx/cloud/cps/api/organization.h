// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx::cloud::cps::api {

namespace Permissions {

// Allows binding/unbinding systems to/from organization.
static constexpr char kManageSystems[] = "manage_systems";

} // namespace Permissions

struct Organization
{
    std::string id;
    std::string name;

    // Array of permissions from Permissions namespace.
    std::vector<std::string> ownPermissions;
};

NX_REFLECTION_INSTRUMENT(Organization, (id)(name)(ownPermissions))

} // namespace nx::cloud::cps::api
