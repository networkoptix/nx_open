#pragma once

#include <nx_ec/data/api_fwd.h>

#include <nx/cloud/db/api/system_data.h>

#include <common/common_globals.h>

namespace nx::cloud::db {

namespace api { class SystemSharing; }

namespace ec2 {

api::SystemAccessRole permissionsToAccessRole(GlobalPermissions permissions);
void accessRoleToPermissions(
    api::SystemAccessRole accessRole,
    GlobalPermissions* const permissions,
    bool* const isAdmin);

void convert(const api::SystemSharing& from, vms::api::UserData* const to);
void convert(const vms::api::UserData& from, api::SystemSharing* const to);
void convert(const api::SystemSharing& from, nx::vms::api::IdData* const to);

} // namespace ec2
} // namespace nx::cloud::db
