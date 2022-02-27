// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_event.h"

namespace nx::vms::rules {

BasicEvent::BasicEvent(nx::vms::api::rules::EventInfo &info)
{
    m_type = info.eventType;
}

BasicEvent::BasicEvent(const QString &type)
{
    m_type = type;
}

QString BasicEvent::type() const
{
    return m_type;
}

} // namespace nx::vms::rules