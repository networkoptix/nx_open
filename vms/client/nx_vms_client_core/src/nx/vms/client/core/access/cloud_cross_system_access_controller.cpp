// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_access_controller.h"

namespace nx::vms::client::core {

CloudCrossSystemAccessController::CloudCrossSystemAccessController(
    SystemContext* systemContext, QObject* parent):
    base_type(systemContext, parent)
{
}

CloudCrossSystemAccessController::~CloudCrossSystemAccessController()
{
}

} // namespace nx::vms::client::core
