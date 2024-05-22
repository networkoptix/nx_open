// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "router.h"

#include <nx/utils/log/log.h>
#include <nx/vms/rules/basic_action.h>

namespace nx::vms::rules {

Router::Router()
{
}

Router::~Router()
{
}

void Router::receiveAction(const ActionPtr& action)
{
    NX_VERBOSE(this, "Received action: %1 from peer: %2", action->type(), action->originPeerId());
    emit actionReceived(action);
}

} // namespace nx::vms::rules
