// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/access/access_controller.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CloudCrossSystemAccessController: public AccessController
{
    using base_type = AccessController;

public:
    CloudCrossSystemAccessController(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~CloudCrossSystemAccessController();

    using base_type::updateAllPermissions;
};

} // namespace nx::vms::client::core
