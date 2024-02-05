// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "running_event_watcher.h"

#include <nx/vms/rules/basic_event.h>

namespace nx::vms::rules {

namespace {

QString runningEventKey(const EventPtr& event)
{
    return QString{"%1_%2"}.arg(event->type(), event->resourceKey());
}

} // namespace

bool RunningEventWatcher::isRunning(const EventPtr& event) const
{
    return m_events.contains(runningEventKey(event));
}

void RunningEventWatcher::add(const EventPtr& event)
{
    if (!event
        || !NX_ASSERT(!isRunning(event), "Given event is added already")
        || !NX_ASSERT(event->state() == State::started, "Only started events might be added"))
    {
        return;
    }

    m_events.insert({runningEventKey(event), event->timestamp()});
}

void RunningEventWatcher::erase(const EventPtr& event)
{
    if (!event
        || !NX_ASSERT(isRunning(event), "Given event was not added to the watcher")
        || !NX_ASSERT(event->state() == State::stopped, "Only stopped events might be erased"))
    {
        return;
    }

    m_events.erase(runningEventKey(event));
}

std::chrono::microseconds RunningEventWatcher::startTime(const EventPtr& event) const
{
    if (!event)
        std::chrono::microseconds::zero();

    const auto it = m_events.find(runningEventKey(event));
    return it != m_events.end() ? it->second : std::chrono::microseconds::zero();
}

} // namespace nx::vms::rules
