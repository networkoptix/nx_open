#pragma once

#include "api_data.h"

namespace ec2 {

struct ApiUserRoleData: nx::vms::api::IdData
{
    ApiUserRoleData() = default;

    ApiUserRoleData(const QnUuid& id, const QString& name, Qn::GlobalPermissions permissions):
        nx::vms::api::IdData(id),
        name(name),
        permissions(permissions)
    {
    }

    QString name;
    Qn::GlobalPermissions permissions = Qn::NoGlobalPermissions;

    bool isNull() const;
};
#define ApiUserRoleData_Fields IdData_Fields (name)(permissions)

/* Struct is not inherited from ApiUserRoleData as it has no 'id' field. */
struct ApiPredefinedRoleData: nx::vms::api::Data
{
    ApiPredefinedRoleData() = default;

    ApiPredefinedRoleData(const QString& name, Qn::GlobalPermissions permissions, bool isOwner = false):
        name(name),
        permissions(permissions),
        isOwner(isOwner)
    {
    }

    QString name;
    Qn::GlobalPermissions permissions = Qn::NoGlobalPermissions;
    bool isOwner = false;
};
#define ApiPredefinedRoleData_Fields (name)(permissions)(isOwner)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ApiUserRoleData)(ApiPredefinedRoleData), (eq));

} //namespace ec2
