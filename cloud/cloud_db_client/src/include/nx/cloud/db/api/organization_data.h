// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/reflect/instrument.h>

namespace nx::cloud::db::api {

/**
 * Organization update data.
 */
class OrganizationUpdateData
{
public:
    /**%apidoc Organization name. */
    std::string name;
};

NX_REFLECTION_INSTRUMENT(OrganizationUpdateData, (name))

} // namespace nx::cloud::db::api
