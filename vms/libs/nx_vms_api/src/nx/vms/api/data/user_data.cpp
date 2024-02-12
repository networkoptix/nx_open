// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

#include "user_group_data.h"

namespace nx::vms::api {

const QString UserData::kResourceTypeName = lit("User");
const nx::Uuid UserData::kResourceTypeId =
    ResourceData::getFixedTypeId(UserData::kResourceTypeName);

constexpr const char* UserData::kCloudPasswordStub;

void UserData::fillId()
{
    // ATTENTION: This logic is similar to QnUserResource::fillId().
    if (type == UserType::cloud)
    {
        if (!email.isEmpty())
            id = nx::Uuid::fromArbitraryData(email);
        else
            id = nx::Uuid();
    }
    else
    {
        id = nx::Uuid::createUuid();
    }
}

bool UserData::isAdministrator() const
{
    return nx::utils::find_if(groupIds, [](const auto id) { return id == kAdministratorsGroupId; });
}

QString toString(UserType type)
{
    return QJson::serialized(type);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserData, (ubjson)(json)(xml)(sql_record)(csv_record), UserData_Fields)

} // namespace nx::vms::api
