// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_ip_conflict_event.h"

#include <QtNetwork/QHostAddress>

#include <nx/vms/common/html/html.h>

#include "../utils/event_details.h"

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
    std::chrono::microseconds timestamp,
    QnUuid serverId,
    const QHostAddress& address,
    const QStringList& macAddrList)
    :
    base_type(timestamp),
    m_serverId(serverId),
    m_ipAddress(address.toString()),
    m_macAddresses(macAddrList)
{
}

QMap<QString, QString> DeviceIpConflictEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());

    return result;
}

QString DeviceIpConflictEvent::detailing() const
{
    QStringList result;

    result << tr("Conflicting Address: %1").arg(m_ipAddress);
    int n = 0;
    for (const auto& macAddress: m_macAddresses)
        result << tr("MAC #%1: %2").arg(++n).arg(macAddress);

    return result.join(common::html::kLineBreak);
}

} // namespace nx::vms::rules
