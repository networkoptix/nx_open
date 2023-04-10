// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/data/user_group_data.h>

#include "permission_converter.h"

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(UserModelBase, UserModelBase_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelBase, (csv_record)(json)(ubjson)(xml))

UserDataEx UserModelBase::toUserData() &&
{
    UserDataEx user;
    user.id = std::move(id);
    user.name = std::move(name);
    user.type = std::move(type);
    if (user.type == UserType::cloud)
    {
        user.digest = UserData::kCloudPasswordStub;
        user.hash = UserData::kCloudPasswordStub;
    }
    else if (!password)
    {
        if (digest)
            user.digest = std::move(*digest);
        if (hash)
            user.hash = std::move(*hash);
        if (cryptSha512Hash)
            user.cryptSha512Hash = std::move(*cryptSha512Hash);
    }
    user.fullName = std::move(fullName);
    user.permissions = std::move(permissions);
    user.email = std::move(email);
    user.isEnabled = std::move(isEnabled);
    if (password)
        user.password = std::move(*password);
    if (!isHttpDigestEnabled)
        user.digest = UserData::kHttpIsDisabledStub;
    if (isHttpDigestEnabled && user.digest == UserData::kHttpIsDisabledStub)
        user.digest.clear();
    return user;
}

UserModelBase UserModelBase::fromUserData(UserData&& baseData)
{
    UserModelBase model;
    model.id = std::move(baseData.id);
    model.name = std::move(baseData.name);
    model.type = std::move(baseData.type);
    model.fullName = std::move(baseData.fullName);
    model.permissions = std::move(baseData.permissions);
    model.email = std::move(baseData.email);
    model.isHttpDigestEnabled = (model.type == UserType::cloud)
        ? false
        : (baseData.digest != UserData::kHttpIsDisabledStub);
    model.isEnabled = std::move(baseData.isEnabled);
    model.digest = std::move(baseData.digest);
    model.hash = std::move(baseData.hash);
    model.cryptSha512Hash = std::move(baseData.cryptSha512Hash);
    return model;
}

QN_FUSION_ADAPT_STRUCT(UserModelV1, UserModelV1_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelV1, (csv_record)(json)(ubjson)(xml))

UserModelV1::DbUpdateTypes UserModelV1::toDbTypes() &&
{
    auto user = std::move(*this).toUserData();
    if (externalId)
        user.externalId = std::move(*externalId);
    if (isOwner)
        user.groupIds.push_back(kAdministratorsGroupId);
    if (!userRoleId.isNull())
        user.groupIds.push_back(std::move(userRoleId));

    auto accessRights =
        PermissionConverter::accessRights(&user.permissions, user.id, accessibleResources);
    return {std::move(user), std::move(accessRights)};
}

std::vector<UserModelV1> UserModelV1::fromDbTypes(DbListTypes data)
{
    auto& baseList = std::get<std::vector<UserData>>(data);
    auto& allAccessRights = std::get<AccessRightsDataList>(data);

    std::vector<UserModelV1> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserModelV1 model;
        static_cast<UserModelBase&>(model) = fromUserData(std::move(baseData));

        model.isOwner = baseData.isAdministrator();
        if (!baseData.externalId.isEmpty())
            model.externalId = std::move(baseData.externalId);
        for (const auto& id: baseData.groupIds)
        {
            // TODO: #vkutin Convert predefined groups to target global permissions.
            if (!kPredefinedGroupIds.contains(id))
            {
                model.userRoleId = id;
                break;
            }
        }

        PermissionConverter::extractFromResourceAccessRights(
            allAccessRights, model.id, &model.permissions, &model.accessibleResources);
        result.push_back(std::move(model));
    }
    return result;
}

QN_FUSION_ADAPT_STRUCT(UserModelV3, UserModelV3_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelV3, (csv_record)(json)(ubjson)(xml))

UserModelV3::DbUpdateTypes UserModelV3::toDbTypes() &&
{
    auto user = std::move(*this).toUserData();
    user.groupIds = std::move(groupIds);
    if (externalId)
        user.externalId = std::move(*externalId);

    std::optional<AccessRightsData> accessRights;
    if (resourceAccessRights)
    {
        AccessRightsData data;
        data.userId = user.id;
        data.resourceRights = *std::move(resourceAccessRights);
        data.checkResourceExists = CheckResourceExists::no;
        accessRights = std::move(data);
    }

    return {std::move(user), std::move(accessRights)};
}

std::vector<UserModelV3> UserModelV3::fromDbTypes(DbListTypes data)
{
    auto& baseList = std::get<std::vector<UserData>>(data);
    auto& allAccessRights = std::get<AccessRightsDataList>(data);

    std::vector<UserModelV3> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserModelV3 model;
        static_cast<UserModelBase&>(model) = fromUserData(std::move(baseData));

        model.groupIds = std::move(baseData.groupIds);
        model.externalId = std::move(baseData.externalId);
        auto accessRights = nx::utils::find_if(allAccessRights,
            [id = model.getId()](const auto& accessRights) { return accessRights.userId == id; });
        if (accessRights)
            model.resourceAccessRights = std::move(accessRights->resourceRights);

        result.push_back(std::move(model));
    }
    return result;
}

} // namespace nx::vms::api
