// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "running_event_watcher.h"

#include "basic_event.h"
#include "utils/type.h"

namespace nx::vms::rules {

namespace {

QString runningEventKey(const EventPtr& event)
{
    return utils::makeKey(event->type(), event->resourceKey());
}

} // namespace

RunningEventWatcher::RunningEventWatcher(RunningRuleInfo& info):
    m_info(info)
{}

bool RunningEventWatcher::isRunning(const EventPtr& event) const
{
    return m_info.events.contains(runningEventKey(event));
}

size_t RunningEventWatcher::runningResourceCount() const
{
    return m_info.events.size();
}

void RunningEventWatcher::add(const EventPtr& event)
{
    if (!event
        || !NX_ASSERT(!isRunning(event), "Given event is added already")
        || !NX_ASSERT(event->state() == State::started, "Only started events might be added"))
    {
        return;
    }

    m_info.events.insert({runningEventKey(event), event->timestamp()});
}

void RunningEventWatcher::erase(const EventPtr& event)
{
    if (!event
        || !NX_ASSERT(event->state() == State::stopped, "Only stopped events might be erased")
        || !isRunning(event))
    {
        return;
    }

    m_info.events.erase(runningEventKey(event));
}

std::chrono::microseconds RunningEventWatcher::startTime(const EventPtr& event) const
{
    if (!event)
        return std::chrono::microseconds::zero();

    const auto it = m_info.events.find(runningEventKey(event));
    return it != m_info.events.end() ? it->second : std::chrono::microseconds::zero();
}

} // namespace nx::vms::rules
