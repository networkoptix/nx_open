// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_cache.h"

namespace nx::vms::rules {

namespace {

static const std::chrono::seconds kObjectDetectedProcessingTimeout(1);

} // namespace

bool LastEventData::isReportedRecently() const
{
    return !lastReported.hasExpired(kObjectDetectedProcessingTimeout);
}

void EventCache::cleanupOldEventsFromCache(std::chrono::milliseconds timeout)
{
    if (!m_cleanupTimer.hasExpired(timeout))
        return;
    m_cleanupTimer.restart();

    std::erase_if(
        m_previousEvents,
        [timeout](const auto& pair) { return pair.second.lastEvent.hasExpired(timeout); });
}

bool EventCache::isReportedBefore(const std::string& eventKey) const
{
    auto it = m_previousEvents.find(eventKey);
    return it != m_previousEvents.end() && it->second.lastReported.isValid();
}

bool EventCache::isReportedRecently(const std::string& eventKey) const
{
    const auto it = m_previousEvents.find(eventKey);

    return it != m_previousEvents.end() && it->second.isReportedRecently();
}

LastEventData* EventCache::rememberEvent(const std::string& eventKey)
{
    cleanupOldEventsFromCache();
    auto& data = m_previousEvents[eventKey];
    data.lastEvent.restart();
    return &data;
}

void EventCache::reportEvent(const std::string& eventKey)
{
    if (!eventKey.empty())
        m_previousEvents[eventKey].lastReported.restart();
}

} // namespace nx::vms::rules
