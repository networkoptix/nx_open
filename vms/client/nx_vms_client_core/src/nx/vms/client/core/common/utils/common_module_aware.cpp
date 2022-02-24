// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_module_aware.h"

#include <client_core/client_core_module.h>

namespace nx::vms::client::core {

CommonModuleAware::CommonModuleAware():
    QnCommonModuleAware(qnClientCoreModule->commonModule())
{
}

} // namespace nx::vms::client::core
