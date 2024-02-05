// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <unordered_map>

#include "rules_fwd.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API RunningEventWatcher
{
public:
    /** Returns whether given event is added to the watcher. */
    bool isRunning(const EventPtr& event) const;

    void add(const EventPtr& event);
    void erase(const EventPtr& event);

    /** Returns time when the given event is started or zero if the event was not added. */
    std::chrono::microseconds startTime(const EventPtr& event) const;

private:
    std::unordered_map<QString, std::chrono::microseconds> m_events;
};

} // namespace nx::vms::rules
