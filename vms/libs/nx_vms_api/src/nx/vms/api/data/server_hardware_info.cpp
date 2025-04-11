// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_hardware_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkInterfaceController, (json), NetworkInterfaceController_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServerHardwareInfo, (json), ServerHardwareInfo_Fields)

} // namespace nx::vms::api
