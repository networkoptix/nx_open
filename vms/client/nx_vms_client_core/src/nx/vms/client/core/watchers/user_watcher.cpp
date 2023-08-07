// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_watcher.h"

#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/global_permission_deprecated.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/checked_cast.h>

namespace nx::vms::client::core {

namespace {

void updateUserCompatibilityPermissions(
    const QnUserResourcePtr& user,
    const RemoteConnectionPtr& connection)
{
    if (!NX_ASSERT(connection))
        return;

    const auto compatibilityUserModel = connection->connectionInfo().compatibilityUserModel;
    if (!compatibilityUserModel)
        return;

    NX_ASSERT(connection->moduleInformation().version < nx::utils::SoftwareVersion(6, 0),
        "Compatibility model should not be requested for the 6.0 systems");

    GlobalPermissions permissions;
    std::vector<QnUuid> groupIds;
    std::map<QnUuid, nx::vms::api::AccessRights> resourceAccessRights;
    std::tie(permissions, groupIds, resourceAccessRights) =
        nx::vms::api::migrateAccessRights(
            compatibilityUserModel->permissions,
            compatibilityUserModel->accessibleResources.value_or(std::vector<QnUuid>()),
            compatibilityUserModel->isOwner);

    user->setRawPermissions(permissions);
    user->setGroupIds(groupIds);
    user->setResourceAccessRights(resourceAccessRights);
}

} // namespace

UserWatcher::UserWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    connect(resourcePool(), &QnResourcePool::resourcesAdded,
        this,
        [this](const QnResourceList&)
        {
            const auto username = QString::fromStdString(
                this->systemContext()->connectionCredentials().username);

            if (!m_user && !username.isEmpty())
            {
                // Here we use the same method as server in order to select the correct user when
                // there are multiple users with the same name.
                if (const auto user = resourcePool()->userByName(username).first)
                {
                    updateUserCompatibilityPermissions(user, this->systemContext()->connection());
                    setUser(user);
                }
            }
        }
    );

    connect(resourcePool(), &QnResourcePool::resourcesRemoved,
        this,
        [this](const QnResourceList& resources)
        {
            if (m_user && resources.contains(m_user))
                setUser(QnUserResourcePtr());
        }
    );
}

const QnUserResourcePtr& UserWatcher::user() const
{
    return m_user;
}

void UserWatcher::setUser(const QnUserResourcePtr& user)
{
    if (m_user == user)
        return;

    m_user = user;

    emit userChanged(user);
    emit userNameChanged();
}

QString UserWatcher::userName() const
{
    return m_user ? m_user->getName() : QString();
}

} // namespace nx::vms::client::core
