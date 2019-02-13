#pragma once

#include <nx/vms/api/rules/event_info.h>

#include <chrono>

namespace nx::vms::rules {

class BasicEvent
{
public:
    BasicEvent(nx::vms::api::rules::EventInfo &info);

private:
    std::chrono::milliseconds m_timestamp;
    QStringList m_source;
    bool m_state;
};

} // namespace nx::vms::rules