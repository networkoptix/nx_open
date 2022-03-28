// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_ip_conflict_event.h"

#include <QtNetwork/QHostAddress>

namespace nx::vms::rules {

const ItemDescriptor& DeviceIpConflictEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.deviceIpConflict",
        .displayName = tr("Device IP Conflict"),
        .description = "",
    };
    return kDescriptor;
}

DeviceIpConflictEvent::DeviceIpConflictEvent(
    QnUuid serverId,
    EventTimestamp timestamp,
    const QHostAddress& address,
    const QStringList& macAddrList)
    :
    base_type(timestamp),
    m_serverId(serverId),
    m_ipAddress(address.toString()),
    m_macAddresses(macAddrList)
{
}

} // namespace nx::vms::rules
