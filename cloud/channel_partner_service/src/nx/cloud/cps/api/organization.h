// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
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

struct SsoOrganizationInfoRequest
{
    std::vector<std::string> organizationIds;
};

NX_REFLECTION_INSTRUMENT(SsoOrganizationInfoRequest, (organizationIds))

struct SsoOrganizationInfoItem
{
    std::string id;
    bool ssoRequired = false;
    bool ssoAvailable = false;
};

NX_REFLECTION_INSTRUMENT(SsoOrganizationInfoItem, (id)(ssoRequired)(ssoAvailable))

struct SsoOrganizationInfoResponse
{
    std::vector<SsoOrganizationInfoItem> organizations;
    std::optional<std::vector<std::string>> missingIds;
};

NX_REFLECTION_INSTRUMENT(SsoOrganizationInfoResponse, (organizations)(missingIds))

} // namespace nx::cloud::cps::api
