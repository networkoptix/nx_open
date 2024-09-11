// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <nx/vms/event/event_cache.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API EventCache: public nx::vms::event::EventCache
{
public:
    /** Check prolonged event on-off consistency.*/
    bool checkEventState(const EventPtr& event);

private:
    std::unordered_set<QString> m_runningEvents;
};


} // namespace nx::vms::rules
