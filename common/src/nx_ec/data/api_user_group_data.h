#pragma once

#include "api_data.h"

namespace ec2 {
struct ApiUserGroupData: ApiIdData
{
    ApiUserGroupData():
        name(),
        permissions(Qn::NoGlobalPermissions)
    {}

    QString name;
    Qn::GlobalPermissions permissions;
};
#define ApiUserGroupData_Fields ApiIdData_Fields (name)(permissions)

/* Struct is not inherited from ApiUserGroupData as it has no 'id' field. */
struct ApiPredefinedRoleData: ApiData
{
    ApiPredefinedRoleData():
        name(),
        permissions(Qn::NoGlobalPermissions),
        isOwner(false)
    {}

    ApiPredefinedRoleData(const QString& name, Qn::GlobalPermissions permissions, bool isOwner = false):
        name(name),
        permissions(permissions),
        isOwner(isOwner)
    {}

    QString name;
    Qn::GlobalPermissions permissions;
    bool isOwner;
};
#define ApiPredefinedRoleData_Fields (name)(permissions)(isOwner)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ApiUserGroupData)(ApiPredefinedRoleData), (eq));
}
