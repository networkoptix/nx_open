// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <nx/utils/impl_ptr.h>
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

    void setTokenHelper(nx::vms::common::SessionTokenHelperPtr tokenHelper);
    nx::vms::common::SessionTokenHelperPtr tokenHelper() const;

    bool isRunning() const;

    static void updateLayoutSharing(
        nx::vms::client::desktop::SystemContext* systemContext,
        const std::map<QnUuid, nx::vms::api::AccessRights>& accessRights);

protected:
    virtual void makeRequest() override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
