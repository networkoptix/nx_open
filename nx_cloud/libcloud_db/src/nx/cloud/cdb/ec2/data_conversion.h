#pragma once

#include <nx/cloud/cdb/api/system_data.h>

#include <common/common_globals.h>

namespace ec2 {

struct ApiUserData;
struct ApiIdData;

} // namespace ec2

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
void convert(const api::SystemSharing& from, ::ec2::ApiIdData* const to);

} // namespace ec2
} // namespace cdb
} // namespace nx
