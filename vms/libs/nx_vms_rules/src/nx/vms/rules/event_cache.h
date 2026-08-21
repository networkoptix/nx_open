// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <unordered_map>

#include <nx/utils/elapsed_timer.h>

namespace nx::vms::rules {

/*
 * A single analytics event could contains not all necessary data to match it.
 * Some fields could be at the previous events only. So, we need to collect information
 * related to the analytics track and keep it at this context. Event could use context
 * to match its attributes (in the future) or its objectType (current version).
 */
struct NX_VMS_RULES_API LastEventData
{
    /** Object type which was detected during the track. */
    QString objectTypeId;

    nx::utils::ElapsedTimer lastEvent{ nx::utils::ElapsedTimerState::started };
    nx::utils::ElapsedTimer lastReported{ nx::utils::ElapsedTimerState::invalid };

    bool isReportedRecently() const;
};

/** Simple cache to discard frequent duplicate events. */
class NX_VMS_RULES_API EventCache
{
public:
    LastEventData* rememberEvent(const std::string& eventKey);
    void reportEvent(const std::string& eventKey);

    bool isReportedBefore(const std::string& eventKey) const;
    bool isReportedRecently(const std::string& eventKey) const;

    void cleanupOldEventsFromCache(std::chrono::milliseconds timeout = std::chrono::seconds(60));

private:
    std::unordered_map<std::string, LastEventData> m_previousEvents;
    nx::utils::ElapsedTimer m_cleanupTimer;
};

} // namespace nx::vms::rules
