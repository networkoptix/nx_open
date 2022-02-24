// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVector>

#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <unordered_set>
#include <memory>
#include <functional>

class QnUserRolesManager;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using UserRolesObserver = std::function<void(const QnUuid&)>;
using UserRolesObserverPtr = std::shared_ptr<UserRolesObserver>;
using UserRolesObserverWeakPtr = std::weak_ptr<UserRolesObserver>;

class UserRolesProvider
{
public:
    UserRolesProvider(QnUserRolesManager* userRolesManager);

    QVector<QnUuid> getAllRoles();
    void installAddRoleObserver(UserRolesObserverPtr& observer);
    void installRemoveRoleObserver(UserRolesObserverPtr& observer);

private:
    const QnUserRolesManager* m_userRolesManager;
    std::list<UserRolesObserverWeakPtr> m_roleAddObservers;
    std::list<UserRolesObserverWeakPtr> m_roleRemoveObservers;
    std::unordered_set<QnUuid> m_roles;
    nx::utils::ScopedConnections m_connectionsGuard;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
