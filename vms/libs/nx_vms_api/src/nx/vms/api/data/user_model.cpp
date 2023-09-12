// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_model.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/random.h>
#include <nx/utils/std/algorithm.h>

#include "user_data_deprecated.h"
#include "user_group_data.h"

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT(TemporaryToken, TemporaryToken_Fields)
QN_FUSION_DEFINE_FUNCTIONS(TemporaryToken, (json))

QN_FUSION_ADAPT_STRUCT(UserModelBase, UserModelBase_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelBase, (json))

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
    user.email = std::move(email);
    user.isEnabled = std::move(isEnabled);
    if (password)
        user.password = std::move(*password);
    if (externalId)
        user.externalId = std::move(*externalId);
    user.attributes = attributes;
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
    model.email = std::move(baseData.email);
    model.isHttpDigestEnabled = (baseData.digest != UserData::kHttpIsDisabledStub);
    model.isEnabled = std::move(baseData.isEnabled);
    if (!baseData.externalId.dn.isEmpty())
        model.externalId = std::move(baseData.externalId);
    if (baseData.attributes != UserAttributes{})
        model.attributes = std::move(baseData.attributes);
    model.digest = std::move(baseData.digest);
    model.hash = std::move(baseData.hash);
    model.cryptSha512Hash = std::move(baseData.cryptSha512Hash);
    return model;
}

QN_FUSION_ADAPT_STRUCT(UserModelV1, UserModelV1_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelV1, (json))

// This method doesn't preserve LDAP `groupIds` which are immutable for PATCH requests.
UserModelV1::DbUpdateTypes UserModelV1::toDbTypes() &&
{
    auto user = std::move(*this).toUserData();
    std::tie(user.permissions, user.groupIds, user.resourceAccessRights) = migrateAccessRights(
        permissions, accessibleResources.value_or(std::vector<QnUuid>{}), isOwner);
    if (!userRoleId.isNull())
        user.groupIds.push_back(std::move(userRoleId));

    return {std::move(user)};
}

std::vector<UserModelV1> UserModelV1::fromDbTypes(DbListTypes data)
{
    auto& baseList = std::get<std::vector<UserData>>(data);

    std::vector<UserModelV1> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserModelV1 model;
        static_cast<UserModelBase&>(model) = fromUserData(std::move(baseData));

        for (const auto& id: baseData.groupIds)
        {
            if (UserDataDeprecated::groupIdToPermissionPreset(id)
                || id == kAdministratorsGroupId
                || id == kDefaultLdapGroupId)
            {
                continue;
            }

            model.userRoleId = id;
            break;
        }

        std::tie(model.permissions, model.accessibleResources, model.isOwner) =
            extractFromResourceAccessRights(
                baseData.permissions, std::move(baseData.groupIds), baseData.resourceAccessRights);

        result.push_back(std::move(model));
    }

    return result;
}

QN_FUSION_ADAPT_STRUCT(UserModelV3, UserModelV3_Fields)
QN_FUSION_DEFINE_FUNCTIONS(UserModelV3, (json))

UserModelV3::DbUpdateTypes UserModelV3::toDbTypes() &&
{
    auto user = std::move(*this).toUserData();
    user.groupIds = std::move(groupIds);
    user.permissions = std::move(permissions);
    user.resourceAccessRights = std::move(resourceAccessRights);
    if (user.type == UserType::temporaryLocal && NX_ASSERT(temporaryToken))
    {
        QnJsonContext context;
        context.setChronoSerializedAsDouble(true);
        QJson::serialize(&context, *temporaryToken, &user.hash);
    }

    return {std::move(user)};
}

std::vector<UserModelV3> UserModelV3::fromDbTypes(DbListTypes data)
{
    auto& baseList = std::get<std::vector<UserData>>(data);

    std::vector<UserModelV3> result;
    result.reserve(baseList.size());
    for (auto& baseData: baseList)
    {
        UserModelV3 model;
        static_cast<UserModelBase&>(model) = fromUserData(std::move(baseData));

        model.groupIds = std::move(baseData.groupIds);
        model.permissions = std::move(baseData.permissions);
        if (!baseData.resourceAccessRights.empty())
            model.resourceAccessRights = std::move(baseData.resourceAccessRights);

        if (model.type == UserType::temporaryLocal && NX_ASSERT(model.hash))
        {
            QnJsonContext context;
            context.setStrictMode(true);
            TemporaryToken temporaryToken;
            if (NX_ASSERT(QJson::deserialize(&context, *model.hash, &temporaryToken)))
                model.temporaryToken = std::move(temporaryToken);
        }

        result.push_back(std::move(model));
    }

    return result;
}

void TemporaryToken::generateToken()
{
    token = kPrefix + nx::utils::random::generateName(10);
}

bool TemporaryToken::isValid() const
{
    using namespace std::chrono;
    return endS > 0s && !token.empty();
}

} // namespace nx::vms::api
