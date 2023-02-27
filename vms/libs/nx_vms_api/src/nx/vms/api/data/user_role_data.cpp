// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_role_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

UserRoleData::UserRoleData(
    const QnUuid& id,
    const QString& name,
    GlobalPermissions permissions,
    std::vector<QnUuid> parentGroupIds)
    :
    IdData(id),
    name(name),
    permissions(permissions),
    parentGroupIds(std::move(parentGroupIds))
{
}

UserRoleData UserRoleData::makePredefined(
    const QnUuid& id,
    const QString& name,
    const QString& description,
    GlobalPermissions permissions)
{
    UserRoleData role(id, name, permissions);
    role.description = description;
    role.isPredefined = true;
    return role;
}

QString UserRoleData::toString() const
{
    return QJson::serialized(*this);
}

const QnUuid UserRoleData::kLdapDefaultId("00000000-0000-0000-0000-100100000000");

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserRoleData, (ubjson)(json)(xml)(sql_record)(csv_record), UserRoleData_Fields)

} // namespace nx::vms::api
