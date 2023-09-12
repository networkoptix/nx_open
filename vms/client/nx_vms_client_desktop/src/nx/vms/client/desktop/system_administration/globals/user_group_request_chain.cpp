// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_group_request_chain.h"

#include <boost/iterator/transform_iterator.hpp>

#include <api/server_rest_connection.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

UserGroupRequestChain::UserGroupRequestChain(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    base_type(systemContext)
{
}

UserGroupRequestChain::~UserGroupRequestChain()
{
    if (m_currentRequest != -1)
        systemContext()->connectedServerApi()->cancelRequest(m_currentRequest);
}

nx::vms::common::SessionTokenHelperPtr UserGroupRequestChain::tokenHelper() const
{
    return m_tokenHelper
        ? m_tokenHelper
        : systemContext()->restApiHelper()->getSessionTokenHelper();
}

void UserGroupRequestChain::makeRequest(const UserGroupRequest::Type& data)
{
    NX_ASSERT(m_currentRequest == -1);

    if (const auto mod = std::get_if<UserGroupRequest::ModifyGroupParents>(&data))
        requestModifyGroupParents(*mod);
    else if (const auto mod = std::get_if<UserGroupRequest::ModifyUserParents>(&data))
        requestModifyUserParents(*mod);
    else if (const auto updateData = std::get_if<UserGroupRequest::AddOrUpdateGroup>(&data))
        requestSaveGroup(*updateData);
    else if (const auto removalData = std::get_if<UserGroupRequest::RemoveGroup>(&data))
        requestRemoveGroup(*removalData);
    else if (const auto updateData = std::get_if<UserGroupRequest::RemoveUser>(&data))
        requestRemoveUser(*updateData);
    else if (const auto updateData = std::get_if<UserGroupRequest::UpdateUser>(&data))
        requestUpdateUser(*updateData);
}

void UserGroupRequestChain::requestModifyGroupParents(
    const UserGroupRequest::ModifyGroupParents& mod)
{
    auto groupData = fromGroupId(mod.id);
    if (groupData.id.isNull())
    {
        requestComplete(false, tr("Group does not exist"));
        return;
    }

    groupData.parentGroupIds = mod.newParents;

    m_currentRequest = connectedServerApi()->saveGroupAsync(
        /*newGroup*/ false,
        groupData,
        tokenHelper(),
        nx::utils::guarded(this,
            [this, mod](
                bool success,
                int handle,
                const rest::ErrorOrData<nx::vms::api::UserGroupModel>& errorOrData)
            {
                if (NX_ASSERT(handle == m_currentRequest))
                    m_currentRequest = -1;

                QString errorString;

                if (success)
                    applyGroup(mod.id, mod.prevParents, mod.newParents);
                else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                    errorString = error->errorString;

                requestComplete(success, errorString);
            }),
        thread());
}

void UserGroupRequestChain::requestModifyUserParents(
    const UserGroupRequest::ModifyUserParents& mod)
{
    nx::vms::api::UserModelV3 userData = fromUserId(mod.id);
    if (userData.id.isNull())
    {
        requestComplete(false, tr("User does not exist"));
        return;
    }

    userData.groupIds = mod.newParents;

    m_currentRequest = connectedServerApi()->saveUserAsync(
        /*newUser*/ false,
        userData,
        tokenHelper(),
        nx::utils::guarded(this,
            [this, mod](
                bool success,
                int handle,
                const rest::ErrorOrData<nx::vms::api::UserModelV3>& errorOrData)
            {
                if (NX_ASSERT(handle == m_currentRequest))
                    m_currentRequest = -1;

                QString errorString;

                if (success)
                    applyUser(mod.id, mod.prevParents, mod.newParents);
                else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                    errorString = error->errorString;

                requestComplete(success, errorString);
            }),
        thread());
}

void UserGroupRequestChain::requestSaveGroup(const UserGroupRequest::AddOrUpdateGroup& data)
{
    const auto onGroupSaved = nx::utils::guarded(this,
        [this, data](
            bool success,
            int handle,
            rest::ErrorOrData<nx::vms::api::UserGroupModel> errorOrData)
        {
            if (NX_ASSERT(handle == m_currentRequest))
                m_currentRequest = -1;

            QString errorString;

            if (auto userGroup = std::get_if<nx::vms::api::UserGroupModel>(&errorOrData);
                userGroup && success)
            {
                auto group =
                    systemContext()->userGroupManager()->find(
                        data.groupData.id).value_or(api::UserGroupData{});

                // Update group locally ahead of receiving update from the server
                // to avoid UI blinking.
                group.id = userGroup->id;
                group.name = userGroup->name;
                group.description = userGroup->description;
                group.parentGroupIds = userGroup->parentGroupIds;
                group.permissions = userGroup->permissions;
                userGroupManager()->addOrUpdate(group);

                updateLayoutSharing(
                    systemContext(), userGroup->id, userGroup->resourceAccessRights);

                // Update access rights locally.
                systemContext()->accessRightsManager()->setOwnResourceAccessMap(userGroup->id,
                    {userGroup->resourceAccessRights.begin(), userGroup->resourceAccessRights.end()});
            }
            else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
            {
                errorString = error->errorString;
            }

            requestComplete(success, errorString);
        });

    m_currentRequest = connectedServerApi()->saveGroupAsync(
        data.newGroup,
        data.groupData,
        tokenHelper(),
        onGroupSaved,
        thread());
}

void UserGroupRequestChain::requestRemoveUser(const UserGroupRequest::RemoveUser& data)
{
    auto onUserRemoved = nx::utils::guarded(this,
        [this, id = data.id](bool success, int handle, const rest::ErrorOrEmpty& result)
        {
            if (NX_ASSERT(handle == m_currentRequest))
                m_currentRequest = -1;

            QString errorString;

            if (success)
            {
                // Remove if not already removed.
                if (auto resource = systemContext()->resourcePool()->getResourceById(id))
                    systemContext()->resourcePool()->removeResource(resource);
            }
            else if (auto error = std::get_if<nx::network::rest::Result>(&result))
            {
                errorString = error->errorString;
            }

            requestComplete(success, errorString);
        });

    m_currentRequest = connectedServerApi()->removeUserAsync(
        data.id, tokenHelper(), onUserRemoved, thread());
}

void UserGroupRequestChain::requestRemoveGroup(const UserGroupRequest::RemoveGroup& data)
{
    const auto onGroupRemoved = nx::utils::guarded(this,
        [this, id = data.id](bool success, int handle, const rest::ErrorOrEmpty& result)
        {
            if (NX_ASSERT(handle == m_currentRequest))
                m_currentRequest = -1;

            QString errorString;

            if (success)
            {
                // Remove if not already removed.
                userGroupManager()->remove(id);
            }
            else if (auto error = std::get_if<nx::network::rest::Result>(&result))
            {
                errorString = error->errorString;
            }

            requestComplete(success, errorString);
        });

    m_currentRequest = connectedServerApi()->removeGroupAsync(
        data.id, tokenHelper(), onGroupRemoved, thread());
}

void UserGroupRequestChain::requestUpdateUser(const UserGroupRequest::UpdateUser& data)
{
    nx::vms::api::UserModelV3 userData = fromUserId(data.id);
    if (userData.id.isNull())
    {
        requestComplete(false, tr("User does not exist"));
        return;
    }

    userData.isEnabled = data.enabled;
    userData.isHttpDigestEnabled = data.enableDigest;

    m_currentRequest = connectedServerApi()->saveUserAsync(
        /*newUser*/ false,
        userData,
        tokenHelper(),
        nx::utils::guarded(this,
            [this](
                bool success,
                int handle,
                const rest::ErrorOrData<nx::vms::api::UserModelV3>& errorOrData)
            {
                if (NX_ASSERT(handle == m_currentRequest))
                    m_currentRequest = -1;

                QString errorString;

                if (auto userData = std::get_if<nx::vms::api::UserModelV3>(&errorOrData);
                    userData && success)
                {
                    applyUser(userData->id, userData->isEnabled, userData->isHttpDigestEnabled);
                }
                else if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                {
                    errorString = error->errorString;
                }

                requestComplete(success, errorString);
            }),
        thread());
}

void UserGroupRequestChain::updateLayoutSharing(
    nx::vms::client::desktop::SystemContext* systemContext,
    const QnUuid& id,
    const std::map<QnUuid, nx::vms::api::AccessRights>& accessRights)
{
    const auto resourcePool = systemContext->resourcePool();

    const auto getKey = [](const auto& pair) { return pair.first; };

    const auto keys = nx::utils::rangeAdapter(
        boost::make_transform_iterator(accessRights.cbegin(), getKey),
        boost::make_transform_iterator(accessRights.cend(), getKey));

    const auto layouts = resourcePool->getResourcesByIds(keys).filtered<LayoutResource>(
        [](const QnLayoutResourcePtr& layout)
        {
            return !layout->isFile()
                && !layout->isShared()
                && !layout->hasFlags(Qn::cross_system);
        });

    for (const auto& layout: layouts)
    {
        layout->setParentId(QnUuid());
        auto resourceSystemContext = SystemContext::fromResource(layout);
        if (!NX_ASSERT(resourceSystemContext))
            continue;

        resourceSystemContext->layoutSnapshotManager()->save(layout);
    }
}

nx::vms::api::UserGroupModel UserGroupRequestChain::fromGroupId(const QnUuid& groupId)
{
    const auto userGroup = systemContext()->userGroupManager()->find(groupId);
    if (!userGroup)
        return {};

    nx::vms::api::UserGroupModel groupData;

    // Fill in all non-optional fields.
    groupData.id = userGroup->id;
    groupData.name = userGroup->name;
    groupData.description = userGroup->description;
    groupData.type = userGroup->type;
    groupData.permissions = userGroup->permissions;
    groupData.parentGroupIds = userGroup->parentGroupIds;
    const auto ownResourceAccessMap = systemContext()->accessRightsManager()->ownResourceAccessMap(
        userGroup->id).asKeyValueRange();
    groupData.resourceAccessRights = {ownResourceAccessMap.begin(), ownResourceAccessMap.end()};
    return groupData;
}

nx::vms::api::UserModelV3 UserGroupRequestChain::fromUserId(const QnUuid& userId)
{
    const auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return {};

    nx::vms::api::UserModelV3 userData;

    // Fill in all non-optional fields.
    userData.id = user->getId();
    userData.name = user->getName();
    userData.email = user->getEmail();
    userData.type = user->userType();
    userData.fullName = user->fullName();
    userData.permissions = user->getRawPermissions();
    userData.isEnabled = user->isEnabled();
    userData.isHttpDigestEnabled =
        user->getDigest() != nx::vms::api::UserData::kHttpIsDisabledStub;

    userData.groupIds = user->groupIds();

    const auto ownResourceAccessMap = systemContext()->accessRightsManager()->ownResourceAccessMap(
        user->getId()).asKeyValueRange();
    userData.resourceAccessRights = {ownResourceAccessMap.begin(), ownResourceAccessMap.end()};

    return userData;
}

void UserGroupRequestChain::applyGroup(
    const QnUuid& groupId,
    const std::vector<QnUuid>& prev,
    const std::vector<QnUuid>& next)
{
    auto userGroup = systemContext()->userGroupManager()->find(groupId);
    if (!userGroup)
        return;

    if (userGroup->parentGroupIds != prev)
        return; //< Already updated.

    userGroup->parentGroupIds = next;
    systemContext()->userGroupManager()->addOrUpdate(*userGroup);
}

void UserGroupRequestChain::applyUser(
    const QnUuid& userId,
    const std::vector<QnUuid>& prev,
    const std::vector<QnUuid>& next)
{
    auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return;

    if (user->groupIds() != prev)
        return;

    user->setGroupIds(next);
}

void UserGroupRequestChain::applyUser(
    const QnUuid& userId,
    bool enabled,
    bool enableDigest)
{
    auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return;

    // TODO: enableDigest shoud be optional in API structure.

    user->setEnabled(enabled);
    user->setPasswordAndGenerateHash(
        {},
        enableDigest
            ? QnUserResource::DigestSupport::enable
            : QnUserResource::DigestSupport::disable);
}

} // namespace nx::vms::client::desktop
