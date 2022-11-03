// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_model.h"

#include <nx/utils/std/algorithm.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(UserModelBase, UserModelBase_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelBase, (csv_record)(json)(ubjson)(xml))

UserDataEx UserModelBase::toUserData() &&
{
    UserDataEx user;
    user.id = std::move(id);
    user.name = std::move(name);
    user.isAdmin = std::move(isOwner);
    setType(&user, type);
    if (type == UserType::cloud)
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
        if (realm)
            user.realm = std::move(*realm);
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
    model.type = api::type(baseData);
    model.fullName = std::move(baseData.fullName);
    model.isOwner = baseData.isAdmin;
    model.permissions = std::move(baseData.permissions);
    model.email = std::move(baseData.email);
    model.isHttpDigestEnabled = baseData.isCloud
        ? false
        : (baseData.digest != UserData::kHttpIsDisabledStub);
    model.isEnabled = std::move(baseData.isEnabled);
    model.digest = std::move(baseData.digest);
    model.hash = std::move(baseData.hash);
    model.cryptSha512Hash = std::move(baseData.cryptSha512Hash);
    model.realm = std::move(baseData.realm);
    return model;
}

QN_FUSION_ADAPT_STRUCT(UserModelV1, UserModelV1_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelV1, (csv_record)(json)(ubjson)(xml))

UserModelV1::DbUpdateTypes UserModelV1::toDbTypes() &&
{
    auto user = std::move(*this).toUserData();
    if (externalId)
        user.externalId = std::move(*externalId);

    if (!userRoleId.isNull())
        user.userRoleIds.push_back(std::move(userRoleId));

    std::optional<AccessRightsData> accessRights;
    if (accessibleResources)
    {
        AccessRightsData data;
        data.userId = user.id;
        const auto resourceAccessRights = globalPermissionsToAccessRights(user.permissions);
        for (auto& id: *accessibleResources)
            data.resourceRights[std::move(id)] = resourceAccessRights;
        data.checkResourceExists = (user.isAdmin || user.userRoleIds.empty())
            ? CheckResourceExists::no
            : CheckResourceExists::customRole;
        accessRights = std::move(data);
    }

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

        if (!baseData.externalId.isEmpty())
            model.externalId = std::move(baseData.externalId);
        if (!baseData.userRoleIds.empty())
            model.userRoleId = baseData.userRoleIds.front();
        auto accessRights = nx::utils::find_if(allAccessRights,
            [id = model.getId()](const auto& accessRights) { return accessRights.userId == id; });
        if (accessRights)
        {
            std::vector<QnUuid> resources;
            resources.reserve(accessRights->resourceRights.size());
            for (const auto& [id, _]: accessRights->resourceRights)
                resources.push_back(id);
            model.accessibleResources = std::move(resources);
        }

        result.push_back(std::move(model));
    }
    return result;
}

QN_FUSION_ADAPT_STRUCT(UserModelV3, UserModelV3_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelV3, (csv_record)(json)(ubjson)(xml))

UserModelV3::DbUpdateTypes UserModelV3::toDbTypes() &&
{
    auto user = std::move(*this).toUserData();
    user.userRoleIds = std::move(userGroupIds);
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

        model.userGroupIds = std::move(baseData.userRoleIds);
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
