#pragma once
/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class PanicAction: public AbstractAction
{
    using base_type = AbstractAction;

public:
    explicit PanicAction(const EventParameters& runtimeParams);
};

} // namespace event
} // namespace vms
} // namespace nx
