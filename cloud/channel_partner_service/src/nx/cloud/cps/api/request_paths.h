// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::cloud::cps::api {

static constexpr char kSystemBindToOrganizationPath[] = "/partners/cloud_systems/";

static constexpr char kInternalSystemUsersPath[] =
    "/internal/partners/systems/{systemId}/users";

static constexpr char kInternalSystemUserPath[] =
    "/internal/partners/systems/{systemId}/users/{email}";

static constexpr char kInternalUserSystemsPath[] =
    "/internal/partners/users/{email}/systems";

} // namespace nx::cloud::cps::api
