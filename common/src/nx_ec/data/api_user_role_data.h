#pragma once

#include "api_data.h"

#include <nx/vms/api/types/access_rights_types.h>

namespace ec2 {

struct ApiUserRoleData: nx::vms::api::IdData
{
    ApiUserRoleData() = default;

    ApiUserRoleData(const QnUuid& id, const QString& name, GlobalPermissions permissions):
        nx::vms::api::IdData(id),
        name(name),
        permissions(permissions)
    {
    }

    QString name;
    GlobalPermissions permissions;

    bool isNull() const;
};
#define ApiUserRoleData_Fields IdData_Fields (name)(permissions)

/* Struct is not inherited from ApiUserRoleData as it has no 'id' field. */
struct ApiPredefinedRoleData: nx::vms::api::Data
{
    ApiPredefinedRoleData() = default;

    ApiPredefinedRoleData(const QString& name, GlobalPermissions permissions, bool isOwner = false):
        name(name),
        permissions(permissions),
        isOwner(isOwner)
    {
    }

    QString name;
    GlobalPermissions permissions;
    bool isOwner = false;
};
#define ApiPredefinedRoleData_Fields (name)(permissions)(isOwner)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ApiUserRoleData)(ApiPredefinedRoleData), (eq));

} //namespace ec2
