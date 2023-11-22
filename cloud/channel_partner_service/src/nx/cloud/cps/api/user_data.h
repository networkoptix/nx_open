// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx::cloud::cps::api {

// Describes organization's system user.
struct User
{
    std::string email;

    // Channel partner specific roles.
    std::vector<std::string> roles;

    // Roles understandable by VMS.
    std::vector<std::string> vmsRoles;
};

NX_REFLECTION_INSTRUMENT(User, (email)(roles)(vmsRoles))

// Describes user's access to a system.
struct SystemAllowance
{
    std::string system_id;
    std::vector<std::string> vmsRoles;
};

NX_REFLECTION_INSTRUMENT(SystemAllowance, (system_id)(vmsRoles))

} // namespace nx::cloud::cps::api
