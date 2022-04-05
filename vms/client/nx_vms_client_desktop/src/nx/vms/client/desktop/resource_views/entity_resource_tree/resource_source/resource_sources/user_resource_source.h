// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class UserResourceSource: public AbstractResourceSource
{
    Q_OBJECT
    using base_type = AbstractResourceSource;

public:
    UserResourceSource(
        const QnResourcePool* resourcePool,
        const QnUserResourcePtr& currentUser, //< Will be excluded.
        const QnUuid& roleId = QnUuid()); //< Should match with ones from result.

    virtual QVector<QnResourcePtr> getResources() override;

private:
    void onUserRolesChanged(const QnUserResourcePtr& user, const std::vector<QnUuid>& previousRoleId);
    void onEnabledChanged(const QnUserResourcePtr& user);

    const QnResourcePool* m_resourcePool = nullptr;
    const QnUserResourcePtr m_currentUser;
    QnUuid m_roleId;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
