// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_module_aware.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CommonModuleAware: public QnCommonModuleAware
{
public:
    CommonModuleAware();
};

} // namespace nx::vms::client::core
