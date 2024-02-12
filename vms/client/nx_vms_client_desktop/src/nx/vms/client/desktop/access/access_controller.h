// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/vms/client/core/access/access_controller.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::api { struct LayoutData; }

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API AccessController: public client::core::AccessController
{
    Q_OBJECT
    using base_type = client::core::AccessController;

public:
    explicit AccessController(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~AccessController() override;

    /** Resource creation checkers. */
    bool canCreateStorage(const nx::Uuid& serverId) const;
    bool canCreateLayout(const nx::vms::api::LayoutData& layoutData) const;
    bool canCreateUser(
        GlobalPermissions targetPermissions, const std::vector<nx::Uuid>& targetGroups) const;
    bool canCreateVideoWall() const;
    bool canCreateWebPage() const;

    virtual bool isDeviceAccessRelevant(nx::vms::api::AccessRights requiredAccessRights) const;

protected:
    virtual GlobalPermissions calculateGlobalPermissions() const override;

    virtual Qn::Permissions calculatePermissions(
        const QnResourcePtr& targetResource) const override;
private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
