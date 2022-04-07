// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_event.h"

#include <QtCore/QMetaProperty>

#include "utils/type.h"

namespace nx::vms::rules {

BasicEvent::BasicEvent(const nx::vms::api::rules::EventInfo& info):
    m_type(info.eventType),
    m_state(info.state)
{
}

BasicEvent::BasicEvent(std::chrono::microseconds timestamp):
    m_timestamp(timestamp)
{
}

QString BasicEvent::type() const
{
    if (!m_type.isEmpty())
        return m_type;

    return utils::type(metaObject()); //< Assert?
}

} // namespace nx::vms::rules
