// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_group_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

UserGroupData::UserGroupData(
    const nx::Uuid& id,
    const QString& name,
    GlobalPermissions permissions,
    std::vector<nx::Uuid> parentGroupIds)
    :
    IdData(id),
    name(name),
    permissions(permissions),
    parentGroupIds(std::move(parentGroupIds))
{
}

QString UserGroupData::toString() const
{
    return QJson::serialized(*this);
}

const nx::Uuid kAdministratorsGroupId{"00000000-0000-0000-0000-100000000000"};
const nx::Uuid kPowerUsersGroupId{"00000000-0000-0000-0000-100000000001"};
const nx::Uuid kAdvancedViewersGroupId{"00000000-0000-0000-0000-100000000002"};
const nx::Uuid kViewersGroupId{"00000000-0000-0000-0000-100000000003"};
const nx::Uuid kLiveViewersGroupId{"00000000-0000-0000-0000-100000000004"};
const nx::Uuid kSystemHealthViewersGroupId{"00000000-0000-0000-0000-100000000005"};

const nx::Uuid kDefaultLdapGroupId{"00000000-0000-0000-0000-100100000000"};

const std::set<nx::Uuid> kPredefinedGroupIds{
    kAdministratorsGroupId,
    kPowerUsersGroupId,
    kAdvancedViewersGroupId,
    kViewersGroupId,
    kLiveViewersGroupId,
    kSystemHealthViewersGroupId};

const std::set<nx::Uuid> kAllPowerUserGroupIds{
    kAdministratorsGroupId,
    kPowerUsersGroupId};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserGroupData, (ubjson)(json)(xml)(sql_record)(csv_record), UserGroupData_Fields)

} // namespace nx::vms::api
