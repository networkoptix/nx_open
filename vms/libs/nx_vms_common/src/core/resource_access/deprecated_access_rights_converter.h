// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <utils/common/updatable.h>

class QnResourcePool;
class QnUserRolesManager;
class QnSharedResourcesManager;

namespace nx::vms::api { struct AccessRightsData; }

namespace nx::core::access {

class AccessRightsManager;

/**
 * A transition period utility to watch global permissions and shared resources changes
 * and translate them into resource access permission changes.
 */
class NX_VMS_COMMON_API DeprecatedAccessRightsConverter:
    public QObject,
    public QnUpdatable
{
    using base_type = QObject;

public:
    explicit DeprecatedAccessRightsConverter(
        QnResourcePool* resourcePool,
        QnUserRolesManager* userGroupsManager,
        QnSharedResourcesManager* sharedResourcesManager,
        AccessRightsManager* destinationAccessRightsManager,
        QObject* parent = nullptr);

    virtual ~DeprecatedAccessRightsConverter() override;

protected:
    virtual void beforeUpdate() override;
    virtual void afterUpdate() override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
