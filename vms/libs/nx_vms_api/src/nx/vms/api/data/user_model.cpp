// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_model.h"

#include <nx/utils/std/algorithm.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(UserModelBase, UserModelBase_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelBase, (csv_record)(json)(ubjson)(xml))

UserModelBase::DbUpdateTypes UserModelBase::toDbTypesBase() &&
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

    std::optional<AccessRightsData> accessRights;
    if (accessibleResources)
    {
        AccessRightsData data;
        data.userId = user.id;
        data.resourceIds = *std::move(accessibleResources);
        data.checkResourceExists = (user.isAdmin || user.userRoleId.isNull())
            ? CheckResourceExists::no
            : CheckResourceExists::customRole;
        accessRights = std::move(data);
    }

    return {std::move(user), std::move(accessRights)};
}

UserModelBase UserModelBase::fromDbTypesBase(
    UserData&& baseData, AccessRightsDataList&& allAccessRights)
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

    auto accessRights = nx::utils::find_if(allAccessRights,
        [id = model.getId()](const auto& accessRights) { return accessRights.userId == id; });
    if (accessRights)
        model.accessibleResources = std::move(accessRights->resourceIds);

    return model;
}

QN_FUSION_ADAPT_STRUCT(UserModelV3, UserModelV3_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelV3, (csv_record)(json)(ubjson)(xml))

UserModelV3::DbUpdateTypes UserModelV3::toDbTypes() &&
{
    auto result = std::move(*this).toDbTypesBase();
    auto& user = std::get<UserDataEx>(result);
    if (!userRoleId.isNull())
        user.userRoleIds.push_back(std::move(userRoleId));
    user.externalId = std::move(externalId);
    return result;
}

std::vector<UserModelV3> UserModelV3::fromDbTypes(DbListTypes data)
{
    auto& baseList = std::get<std::vector<UserData>>(data);
    auto& accessRights = std::get<AccessRightsDataList>(data);

    std::vector<UserModelV3> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserModelV3 model;
        static_cast<UserModelBase&>(model) =
            fromDbTypesBase(std::move(baseData), std::move(accessRights));
        model.userRoleId = baseData.userRoleIds.empty() ? QUuid() : baseData.userRoleIds.front();
        model.externalId = std::move(baseData.externalId);
        result.push_back(std::move(model));
    }
    return result;
}

QN_FUSION_ADAPT_STRUCT(UserModelVX, UserModelVX_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelVX, (csv_record)(json)(ubjson)(xml))

UserModelV3::DbUpdateTypes UserModelVX::toDbTypes() &&
{
    auto result = std::move(*this).toDbTypesBase();
    auto& user = std::get<UserDataEx>(result);
    user.userRoleIds = std::move(userGroupIds);
    user.externalId = std::move(externalId);
    return result;
}

std::vector<UserModelVX> UserModelVX::fromDbTypes(DbListTypes data)
{
    auto& baseList = std::get<std::vector<UserData>>(data);
    auto& accessRights = std::get<AccessRightsDataList>(data);

    std::vector<UserModelVX> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserModelVX model;
        static_cast<UserModelBase&>(model) =
            fromDbTypesBase(std::move(baseData), std::move(accessRights));

        model.userGroupIds = std::move(baseData.userRoleIds);
        model.externalId = std::move(baseData.externalId);
        result.push_back(std::move(model));
    }
    return result;
}

} // namespace nx::vms::api
