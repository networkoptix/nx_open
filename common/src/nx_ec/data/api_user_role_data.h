#pragma once

#include "api_data.h"

namespace ec2 {

struct ApiUserRoleData: ApiIdData
{
    ApiUserRoleData():
        ApiIdData(),
        name(),
        permissions(Qn::NoGlobalPermissions)
    {
    }

    ApiUserRoleData(const QnUuid& id, const QString& name, Qn::GlobalPermissions permissions):
        ApiIdData(id),
        name(name),
        permissions(permissions)
    {
    }

    QString name;
    Qn::GlobalPermissions permissions;

    bool isNull() const;
};
#define ApiUserRoleData_Fields ApiIdData_Fields (name)(permissions)

/* Struct is not inherited from ApiUserRoleData as it has no 'id' field. */
struct ApiPredefinedRoleData: ApiData
{
    ApiPredefinedRoleData():
        name(),
        permissions(Qn::NoGlobalPermissions),
        isOwner(false)
    {
    }

    ApiPredefinedRoleData(const QString& name, Qn::GlobalPermissions permissions, bool isOwner = false):
        name(name),
        permissions(permissions),
        isOwner(isOwner)
    {
    }

    QString name;
    Qn::GlobalPermissions permissions;
    bool isOwner;
};
#define ApiPredefinedRoleData_Fields (name)(permissions)(isOwner)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ApiUserRoleData)(ApiPredefinedRoleData), (eq));

} //namespace ec2
