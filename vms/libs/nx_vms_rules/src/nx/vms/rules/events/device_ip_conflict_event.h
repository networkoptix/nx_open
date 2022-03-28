// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

class QHostAddress;

namespace nx::vms::rules {

class NX_VMS_RULES_API DeviceIpConflictEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.deviceIpConflict")
    using base_type = BasicEvent;

    FIELD(QnUuid, serverId, setServerId)
    FIELD(QString, ipAddress, setIpAddress)
    FIELD(QStringList, macAddresses, setMacAddresses)

public:
    static const ItemDescriptor& manifest();

    DeviceIpConflictEvent(
        QnUuid deviceId,
        EventTimestamp timestamp,
        const QHostAddress& address,
        const QStringList& macAddrList);
};

} // namespace nx::vms::rules
