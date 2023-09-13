// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <nx/vms/api/data/user_group_model.h>
#include <nx/vms/api/data/user_model.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

#include "request_chain.h"

namespace nx::vms::client::desktop {

struct UserGroupRequest
{
    // A request to remove user by id.
    struct RemoveUser
    {
        QnUuid id;
    };

    // A request to remove group by id.
    struct RemoveGroup
    {
        QnUuid id;
    };

    // A request to modify user parents.
    struct ModifyUserParents
    {
        QnUuid id;
        std::vector<QnUuid> prevParents;
        std::vector<QnUuid> newParents;
    };

    // A request to modify group parents.
    struct ModifyGroupParents
    {
        QnUuid id;
        std::vector<QnUuid> prevParents;
        std::vector<QnUuid> newParents;
    };

    // A request to add or modify group data.
    struct AddOrUpdateGroup
    {
        api::UserGroupModel groupData;
        bool newGroup = false;
        std::vector<QnUuid> originalParents;
    };

    // Update user enabled and digest authentication.
    struct UpdateUser
    {
        QnUuid id;
        bool enabled = true;
        bool enableDigest = false;
    };

    using Type = std::variant<
        ModifyUserParents,
        ModifyGroupParents,
        RemoveUser,
        RemoveGroup,
        AddOrUpdateGroup,
        UpdateUser>;
};

/**
 * Helper class for applying a chain of modifications to users and groups. After each successful
 * request changes are applied immediately to avoid waiting for data from message bus.
 */
class UserGroupRequestChain:
    public QObject,
    public RequestChain<UserGroupRequest::Type>,
    SystemContextAware
{
    Q_OBJECT
    using base_type = SystemContextAware;

public:
    UserGroupRequestChain(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual ~UserGroupRequestChain() override;

    void setTokenHelper(nx::vms::common::SessionTokenHelperPtr tokenHelper)
    {
        m_tokenHelper = tokenHelper;
    }

    bool isRunning() const;

    static void updateLayoutSharing(
        nx::vms::client::desktop::SystemContext* systemContext,
        const QnUuid& id,
        const std::map<QnUuid, nx::vms::api::AccessRights>& accessRights);

protected:
    virtual void makeRequest(const UserGroupRequest::Type& data) override;

private:
    void requestModifyGroupParents(const UserGroupRequest::ModifyGroupParents& mod);
    void requestModifyUserParents(const UserGroupRequest::ModifyUserParents& mod);
    void requestSaveGroup(const UserGroupRequest::AddOrUpdateGroup& data);
    void requestRemoveUser(const UserGroupRequest::RemoveUser& data);
    void requestRemoveGroup(const UserGroupRequest::RemoveGroup& data);
    void requestUpdateUser(const UserGroupRequest::UpdateUser& data);

    nx::vms::api::UserGroupModel fromGroupId(const QnUuid& groupId);
    nx::vms::api::UserModelV3 fromUserId(const QnUuid& userId);

    void applyGroup(
        const QnUuid& groupId,
        const std::vector<QnUuid>& prev,
        const std::vector<QnUuid>& next);

    void applyUser(
        const QnUuid& userId,
        const std::vector<QnUuid>& prev,
        const std::vector<QnUuid>& next);

    void applyUser(
        const QnUuid& userId,
        bool enabled,
        bool enableDigest);

    nx::vms::common::SessionTokenHelperPtr tokenHelper() const;

private:
    nx::vms::common::SessionTokenHelperPtr m_tokenHelper;
    rest::Handle m_currentRequest{};
};

} // namespace nx::vms::client::desktop
