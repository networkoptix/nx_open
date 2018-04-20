#pragma once

#include <nx_ec/data/api_fwd.h>

#include <nx/cloud/cdb/api/system_data.h>

#include <common/common_globals.h>

namespace nx {

namespace api { class SystemSharing; }

namespace cdb {
namespace ec2 {

api::SystemAccessRole permissionsToAccessRole(Qn::GlobalPermissions permissions);
void accessRoleToPermissions(
    api::SystemAccessRole accessRole,
    Qn::GlobalPermissions* const permissions,
    bool* const isAdmin);

void convert(const api::SystemSharing& from, ::ec2::ApiUserData* const to);
void convert(const ::ec2::ApiUserData& from, api::SystemSharing* const to);
void convert(const api::SystemSharing& from, nx::vms::api::IdData* const to);

} // namespace ec2
} // namespace cdb
} // namespace nx
