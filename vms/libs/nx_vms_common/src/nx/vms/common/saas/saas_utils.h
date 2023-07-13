// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::common::saas {

NX_VMS_COMMON_API bool saasIsActive(SystemContext* context);

} // nx::vms::common::saas
