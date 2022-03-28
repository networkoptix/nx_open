// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_failure_event.h"

namespace nx::vms::rules {

ReasonedEvent::ReasonedEvent(
    QnUuid serverId,
    EventTimestamp timestamp,
    EventReason reasonCode,
    const QString& reasonText)
    :
    base_type(timestamp),
    m_serverId(serverId),
    m_reasonCode(reasonCode),
    m_reasonText(reasonText)
{
}

} // namespace nx::vms::rules
