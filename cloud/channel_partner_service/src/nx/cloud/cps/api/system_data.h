// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/client/data/system_data.h>

namespace nx::cloud::cps::api {

/**
 * Information required to register system in cloud.
 */
struct SystemRegistrationRequest: nx::cloud::db::api::SystemRegistrationData
{
    /**%apidoc[opt] Optional organization name. */
    std::string organization;
};

NX_REFLECTION_INSTRUMENT(SystemRegistrationRequest, SystemRegistrationData_Fields(organization))

using SystemRegistrationResponse = nx::cloud::db::api::SystemData;

} // namespace nx::cloud::cps::api
