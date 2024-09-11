#include "event_cache.h"

namespace nx::vms::event {

namespace {

static const std::chrono::seconds kObjectDetectedProcessingTimeout(1);

} // namespace

void EventCache::cleanupOldEventsFromCache(std::chrono::milliseconds timeout)
{
    if (!m_cleanupTimer.hasExpired(timeout))
        return;
    m_cleanupTimer.restart();

    std::erase_if(
        m_previousEvents,
        [timeout](const auto& pair) { return pair.second.lastEvent.hasExpired(timeout); });
}

bool EventCache::isReportedBefore(const QString& eventKey) const
{
    auto it = m_previousEvents.find(eventKey);
    return it != m_previousEvents.end() && it->second.lastReported.isValid();
}

bool EventCache::isReportedRecently(const QString& eventKey) const
{
    const auto it = m_previousEvents.find(eventKey);
    return it != m_previousEvents.end()
        && !it->second.lastReported.hasExpired(kObjectDetectedProcessingTimeout);
}

nx::vms::api::AnalyticsTrackContext* EventCache::rememberEvent(const QString& eventKey)
{
    cleanupOldEventsFromCache();
    auto& data = m_previousEvents[eventKey];
    data.lastEvent.restart();
    return &data;
}

void EventCache::reportEvent(const QString& eventKey)
{
    if (eventKey.isEmpty())
        return;
    m_previousEvents[eventKey].lastReported.restart();
}

} // namespace nx::vms::event
