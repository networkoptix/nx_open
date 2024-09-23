// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QHash>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::event {

// Event key to timer map.
struct NX_VMS_COMMON_API LastEventData: public nx::vms::api::AnalyticsTrackContext
{
    nx::utils::ElapsedTimer lastEvent{ nx::utils::ElapsedTimerState::started };
    nx::utils::ElapsedTimer lastReported{ nx::utils::ElapsedTimerState::invalid };
};

/** Simple cache to discard frequent duplicate events. */
class NX_VMS_COMMON_API EventCache
{
public:
    nx::vms::api::AnalyticsTrackContext* rememberEvent(const QString& eventKey);
    void reportEvent(const QString& eventKey);

    bool isReportedBefore(const QString& eventKey) const;
    bool isReportedRecently(const QString& eventKey) const;

    void cleanupOldEventsFromCache(std::chrono::milliseconds timeout = std::chrono::seconds(60));

private:
    std::map<QString, LastEventData> m_previousEvents;
    nx::utils::ElapsedTimer m_cleanupTimer;
};

} // namespace nx::vms::event
