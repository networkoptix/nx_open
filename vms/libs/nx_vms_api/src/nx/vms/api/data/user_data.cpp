// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

const QString UserData::kResourceTypeName = lit("User");
const QnUuid UserData::kResourceTypeId =
    ResourceData::getFixedTypeId(UserData::kResourceTypeName);

constexpr const char* UserData::kCloudPasswordStub;

void UserData::fillId()
{
    // ATTENTION: This logic is similar to QnUserResource::fillId().
    if (isCloud)
    {
        if (!email.isEmpty())
            id = QnUuid::fromArbitraryData(email);
        else
            id = QnUuid();
    }
    else
    {
        id = QnUuid::createUuid();
    }
}

bool UserData::adaptFromDeprecatedApi()
{
    if (userRoleId.isNull())
        return true;

    if (!userRoleIds.empty())
        return false;

    userRoleIds.push_back(userRoleId);
    userRoleId = QnUuid();
    return true;
}

void UserData::adaptForDeprecatedApi()
{
    if (!userRoleIds.empty())
        userRoleId = userRoleIds.front();
}

QString toString(UserType type)
{
    return QJson::serialized(type);
}

UserType type(const UserData& user)
{
    if (user.isLdap) return UserType::ldap;
    if (user.isCloud) return UserType::cloud;
    return UserType::local;
}

void setType(UserData* user, UserType type)
{
    user->isLdap = false;
    user->isCloud = false;
    switch (type)
    {
        case UserType::local: return;
        case UserType::ldap: user->isLdap = true; return;
        case UserType::cloud: user->isCloud = true; return;
    }

    NX_ASSERT(false, "Unexpected user type: %1", type);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserData, (ubjson)(json)(xml)(sql_record)(csv_record), UserData_Fields)

} // namespace nx::vms::api
