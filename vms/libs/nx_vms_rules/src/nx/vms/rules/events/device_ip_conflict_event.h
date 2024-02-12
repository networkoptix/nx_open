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

    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(QString, ipAddress, setIpAddress)
    FIELD(QStringList, macAddresses, setMacAddresses)

public:
    DeviceIpConflictEvent() = default;
    DeviceIpConflictEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid serverId,
        UuidList deviceIds,
        const QHostAddress& address,
        const QStringList& macAddrList);

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString caption(common::SystemContext* context) const;
    QStringList detailing() const;
    QString extendedCaption(common::SystemContext* context) const;
    QString name(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
