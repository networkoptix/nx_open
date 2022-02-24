// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_model.h"

#include <nx/utils/std/algorithm.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT(UserModel, UserModel_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModel, (csv_record)(json)(ubjson)(xml))

UserModel::DbUpdateTypes UserModel::toDbTypes() &&
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
    user.userRoleId = std::move(userRoleId);
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

std::vector<UserModel> UserModel::fromDbTypes(DbListTypes all)
{
    auto& baseList = std::get<std::vector<UserData>>(all);
    const auto size = baseList.size();
    std::vector<UserModel> result;
    result.reserve(size);
    auto& allAccessRights = std::get<AccessRightsDataList>(all);
    for (auto& baseData: baseList)
    {
        UserModel model;
        model.id = std::move(baseData.id);
        model.name = std::move(baseData.name);
        model.type = api::type(baseData);
        model.fullName = std::move(baseData.fullName);
        model.isOwner = baseData.isAdmin;
        model.permissions = std::move(baseData.permissions);
        model.userRoleId = std::move(baseData.userRoleId);
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

        result.push_back(std::move(model));
    }
    return result;
}

} // namespace api
} // namespace vms
} // namespace nx
