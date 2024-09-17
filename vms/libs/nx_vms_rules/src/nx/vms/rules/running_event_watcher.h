// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <unordered_map>

#include <QtCore/QHash>

#include "rules_fwd.h"

namespace nx::vms::rules {

struct RunningRuleInfo
{
    // Running events by their resource key.
    std::unordered_map<QString, std::chrono::microseconds> events;

    // Running actions by their resources hash.
    QHash<size_t, ActionPtr> actions;
};

class NX_VMS_RULES_API RunningEventWatcher
{
public:
    RunningEventWatcher(RunningRuleInfo& info);

    /** Returns whether given event is added to the watcher. */
    bool isRunning(const EventPtr& event) const;

    /** How many different event resources have running on the rule. */
    size_t runningResourceCount() const;

    void add(const EventPtr& event);
    void erase(const EventPtr& event);

    /** Returns time when the given event is started or zero if the event was not added. */
    std::chrono::microseconds startTime(const EventPtr& event) const;

private:
    RunningRuleInfo& m_info;
};

} // namespace nx::vms::rules
