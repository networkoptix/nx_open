// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::cloud::cps::api {

static constexpr char kSystemBindToOrganizationPath[] = "/partners/api/v2/cloud_systems/";

static constexpr char kUnbindSystemFromOrganizationPath[] =
    "/partners/api/v2/cloud_systems/{systemId}";

static constexpr char kInternalSystemUsersPath[] =
    "/partners/api/v2/internal/systems/{systemId}/users";

static constexpr char kInternalSystemUserPath[] =
    "/partners/api/v2/internal/systems/{systemId}/users/{email}";

static constexpr char kInternalUserSystemsPath[] =
    "/partners/api/v2/internal/users/{email}/systems";

static constexpr char kOrganizationPath[] = "/partners/api/v2/organizations/{organizationId}/";

} // namespace nx::cloud::cps::api
