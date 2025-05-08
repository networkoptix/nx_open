// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/access/access_controller.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CrossSystemAccessController: public AccessController
{
    using base_type = AccessController;

public:
    explicit CrossSystemAccessController(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~CrossSystemAccessController() override;

    using base_type::updateAllPermissions;

protected:
    virtual Qn::Permissions calculatePermissions(
        const QnResourcePtr& targetResource) const override;
};

} // namespace nx::vms::client::core
