// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::api {

const QString UserData::kResourceTypeName = lit("User");
const QnUuid UserData::kResourceTypeId =
    ResourceData::getFixedTypeId(UserData::kResourceTypeName);

// TODO: This value should be in sync with QnPredefinedUserRoles::id(Qn::UserRole::owner).
const QnUuid UserData::kOwnerGroupId("00000000-0000-0000-0000-100000000000");

constexpr const char* UserData::kCloudPasswordStub;

void UserData::fillId()
{
    // ATTENTION: This logic is similar to QnUserResource::fillId().
    if (type == UserType::cloud)
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

bool UserData::isOwner() const
{
    return nx::utils::find_if(groupIds, [](const auto id) { return id == kOwnerGroupId; });
}

QString toString(UserType type)
{
    return QJson::serialized(type);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserData, (ubjson)(json)(xml)(sql_record)(csv_record), UserData_Fields)

} // namespace nx::vms::api
