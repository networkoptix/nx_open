// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user.h"

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/global_permission_deprecated.h>
#include <nx/vms/client/core/system_context.h>

using namespace nx::vms::api;

namespace nx::vms::client::core {

UserResource::UserResource(UserModelV1 data):
    QnUserResource(data.type, data.externalId.value_or(UserExternalIdModel()))
{
    setIdUnsafe(data.id);
    setName(data.name);
    setEnabled(data.isEnabled);
    setEmail(data.email);
    setFullName(data.fullName);

    GlobalPermissions permissions;
    std::vector<nx::Uuid> groupIds;
    std::map<nx::Uuid, AccessRights> resourceAccessRights;
    std::tie(permissions, groupIds, resourceAccessRights) =
        migrateAccessRights(
            data.permissions,
            data.accessibleResources.value_or(std::vector<nx::Uuid>()),
            data.isOwner);

    if (!data.userRoleId.isNull())
        groupIds.push_back(data.userRoleId);

    setRawPermissions(permissions);
    setGroupIds(groupIds);
    setResourceAccessRights(resourceAccessRights);

    m_overwrittenData = std::move(data);
}

UserResource::UserResource(UserModelV3 data):
    QnUserResource(data.type, data.externalId.value_or(UserExternalIdModel()))
{
    setIdUnsafe(data.id);
    setName(data.name);
    setEnabled(data.isEnabled);
    setEmail(data.email);
    setFullName(data.fullName);
    setAttributes(data.attributes);
    setRawPermissions(data.permissions);
    setGroupIds(data.groupIds);
    setResourceAccessRights(data.resourceAccessRights);
}

void UserResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    // Ignore user data changes in the compatibility mode connection.
    if (m_overwrittenData)
        return;

    base_type::updateInternal(source, notifiers);
}

rest::Handle UserResource::saveSettings(
    const nx::vms::api::UserSettings& value,
    common::SessionTokenHelperPtr sessionTokenHelper,
    std::function<void(bool /*success*/, rest::Handle /*handle*/)> callback,
    nx::utils::AsyncHandlerExecutor executor)
{
    auto systemContext = SystemContext::fromResource(toSharedPointer());
    if (!NX_ASSERT(systemContext))
        return rest::Handle{};

    const auto backup = settings();
    setSettings(value);

    nx::vms::api::ResourceWithParameters parameters;
    parameters.setFromParameter({ResourcePropertyKey::User::kUserSettings,
        QString::fromStdString(nx::reflect::json::serialize(value))});

    auto internalCallback = nx::utils::guarded(this,
        [this, executor, callback, backup](
            bool success,
            rest::Handle handle,
            rest::ErrorOrData<nx::vms::api::UserModelV3> /*errorOrData*/)
        {
            if (!success)
                setSettings(backup);

            if (callback)
                executor.submit(callback, success, handle);
        });

    auto api = systemContext->connectedServerApi();
    if (!api)
        return rest::Handle{};

    return api->patchUserParameters(
        getId(),
        parameters,
        sessionTokenHelper,
        internalCallback,
        thread());
}

} // namespace nx::vms::client::core
