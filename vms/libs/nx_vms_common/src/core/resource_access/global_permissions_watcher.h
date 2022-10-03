// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <utils/common/updatable.h>

#include "abstract_global_permissions_watcher.h"

class QnResourcePool;
class QnUserRolesManager;

namespace nx::core::access {

/**
 * A helper class that watches for users and groups and reports their own global permissions
 * and their changes.
 */
class NX_VMS_COMMON_API GlobalPermissionsWatcher:
    public AbstractGlobalPermissionsWatcher,
    public QnUpdatable
{
    Q_OBJECT
    using base_type = AbstractGlobalPermissionsWatcher;

public:
    explicit GlobalPermissionsWatcher(
        QnResourcePool* resourcePool,
        QnUserRolesManager* userGroupManager,
        QObject* parent = nullptr);

    virtual ~GlobalPermissionsWatcher() override;

    virtual nx::vms::api::GlobalPermissions ownGlobalPermissions(
        const QnUuid& subjectId) const override;

protected:
    virtual void beforeUpdate() override;
    virtual void afterUpdate() override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
