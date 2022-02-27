// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_roles_provider.h"

#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

UserRolesProvider::UserRolesProvider(QnUserRolesManager* userRolesManager):
    m_userRolesManager(userRolesManager)
{
    using namespace nx::vms::api;

    auto allRoles = getAllRoles();
    m_roles = std::unordered_set<QnUuid>(std::begin(allRoles), std::end(allRoles));

    const auto notifyObservers =
        [] (std::list<UserRolesObserverWeakPtr>& observersList, const QnUuid& id)
        {
            auto itr = observersList.begin();
            while (itr != observersList.end())
            {
                if (!itr->expired())
                {
                    if (auto observer = itr->lock())
                        (*observer)(id);
                    itr = std::next(itr);
                    continue;
                }
                itr = observersList.erase(itr);
            }
        };

    m_connectionsGuard.add(
        m_userRolesManager->connect(m_userRolesManager, &QnUserRolesManager::userRoleRemoved,
        [this, &notifyObservers](const UserRoleData& userRole)
        {
            m_roles.erase(userRole.id);
            notifyObservers(m_roleRemoveObservers, userRole.id);
        }));

    m_connectionsGuard.add(
        m_userRolesManager->connect(m_userRolesManager, &QnUserRolesManager::userRoleAddedOrUpdated,
        [this, &notifyObservers](const UserRoleData& userRole)
        {
            if (m_roles.find(userRole.id) != std::cend(m_roles))
                return;
            notifyObservers(m_roleAddObservers, userRole.id);
        }));
}

QVector<QnUuid> UserRolesProvider::getAllRoles()
{
    QVector<QnUuid> result;
    auto userRolesData = m_userRolesManager->userRoles();
    for (const auto& data: userRolesData)
        result.append(data.id);
    return result;
}

void UserRolesProvider::installAddRoleObserver(UserRolesObserverPtr& observer)
{
    m_roleAddObservers.push_back(observer);
}

void UserRolesProvider::installRemoveRoleObserver(UserRolesObserverPtr& observer)
{
    m_roleRemoveObservers.push_back(observer);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
