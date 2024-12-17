// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_parameters.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/event/events/abstract_event.h>

namespace nx {
namespace vms {
namespace event {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ActionParameters, (ubjson)(json)(xml)(csv_record), ActionParameters_Fields, (brief, true))

} // namespace event
} // namespace vms
} // namespace nx
