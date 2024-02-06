// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "port_forwarding_configuration.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PortForwardingConfiguration, (json), PortForwardingConfiguration_Fields)

} // namespace nx::vms::api
