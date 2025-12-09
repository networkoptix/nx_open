// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_rights_workaround.h"

#include <api/common_message_processor.h>
#include <api/server_rest_connection.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::mobile {

void UserRightsWorkaround::install(SystemContext* systemContext)
{
    if (!NX_ASSERT(systemContext
        && /*for main system context only*/ systemContext->mode() == SystemContext::Mode::client
        && /*while connection isn't established yet*/!systemContext->user())
        && systemContext->messageProcessor())
    {
        return;
    }

    // Create the workaround object owned by the message processor.
    new UserRightsWorkaround(systemContext, systemContext->messageProcessor());
}

UserRightsWorkaround::UserRightsWorkaround(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    connect(messageProcessor(), &QnCommonMessageProcessor::initialResourcesReceived, this,
        [this]()
        {
            // Check if all user groups the current user recursively inherits from are loaded.
            // Pre 6.0 there was no recursive inheritance, and post 6.1.0 there supposedly
            // will be no groups that aren't loaded at this point.

            const auto user = this->systemContext()->user();
            if (!NX_ASSERT(user))
                return;

            const auto userGroups = user->allGroupIds();
            const bool groupsLoaded = std::all_of(userGroups.cbegin(), userGroups.cend(),
                [this](const nx::Uuid& groupId) { return isUserGroupLoaded(groupId); });

            if (groupsLoaded)
                return;

            connectedServerApi()->getRawResult("/rest/v3/userGroups", {},
                nx::utils::guarded(this,
                    [this](bool success, rest::Handle, QByteArray data, HttpHeaders)
                    {
                        if (!success)
                        {
                            NX_WARNING(this, "UserGroups request failed");
                            return;
                        }

                        api::UserGroupDataList result;
                        if (!QJson::deserialize(data, &result))
                        {
                            NX_WARNING(this, "UserGroups reply cannot be deserialized");
                            return;
                        }

                        userGroupManager()->resetAll(result);

                        for (const auto& group: result)
                        {
                            accessRightsManager()->setOwnResourceAccessMap(group.id,
                                {group.resourceAccessRights.begin(),
                                group.resourceAccessRights.end()});
                        }
                    }),
                thread());
        });
}

bool UserRightsWorkaround::isUserGroupLoaded(const nx::Uuid& groupId) const
{
    const auto group = userGroupManager()->find(groupId);
    if (!group)
        return false;

    for (const auto& parentGroupId: group->parentGroupIds)
    {
        if (!isUserGroupLoaded(parentGroupId))
            return false;
    }

    return true;
}

} // namespace nx::vms::client::mobile
