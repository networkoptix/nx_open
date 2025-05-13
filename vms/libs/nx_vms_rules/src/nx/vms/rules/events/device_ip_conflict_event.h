// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

class QHostAddress;

namespace nx::vms::rules {

class NX_VMS_RULES_API DeviceIpConflictEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "deviceIpConflict")
    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(QString, ipAddress, setIpAddress)
    FIELD(QStringList, macAddresses, setMacAddresses)

public:
    DeviceIpConflictEvent() = default;

    struct DeviceInfo
    {
        nx::Uuid id;
        QString mac;
    };

    /**
     * Register device ip conflict.
     * @param timestamp Event timestamp.
     * @param serverId Event source server.
     * @param devices List of cameras, which have the same IP address. If camera mac address is
     *     unavailable, id is used instead.
     * @param address The IP address, which is assigned to all those cameras.
     */
    DeviceIpConflictEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid serverId,
        QList<DeviceInfo> devices,
        const QHostAddress& address);

    virtual QString aggregationKey() const override { return m_serverId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

    static ItemDescriptor manifest(common::SystemContext* context);

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QString caption(common::SystemContext* context) const;
    QStringList detailing(common::SystemContext* context) const;
    virtual QString name(common::SystemContext* context) const override;
};

} // namespace nx::vms::rules
