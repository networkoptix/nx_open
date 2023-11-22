// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx::cloud::cps::api {

struct User
{
    std::string email;

    // Channel partner specific roles.
    std::vector<std::string> roles;

    // Roles understandable by VMS.
    std::vector<std::string> vmsRoles;
};

NX_REFLECTION_INSTRUMENT(User, (email)(vmsRoles))

} // namespace nx::cloud::cps::api
