// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx::cloud::cps::api {

struct SystemUserDeletion
{
    std::string email;
    std::string systemId;
};

NX_REFLECTION_INSTRUMENT(SystemUserDeletion, (email)(systemId))

struct SystemUserDeletionsRequest
{
    std::vector<SystemUserDeletion> systemUserDeletions;
};

NX_REFLECTION_INSTRUMENT(SystemUserDeletionsRequest, (systemUserDeletions))

} // namespace nx::cloud::cps::api
