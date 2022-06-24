// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API PanicAction: public AbstractAction
{
    using base_type = AbstractAction;

public:
    explicit PanicAction(const EventParameters& runtimeParams);
};

} // namespace event
} // namespace vms
} // namespace nx
