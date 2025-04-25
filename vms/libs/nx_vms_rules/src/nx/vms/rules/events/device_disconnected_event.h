// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API DeviceDisconnectedEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "deviceDisconnected")
    FIELD(nx::Uuid, deviceId, setDeviceId)
    FIELD(nx::Uuid, serverId, setServerId)

public:
    DeviceDisconnectedEvent() = default;
    DeviceDisconnectedEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid deviceId,
        nx::Uuid serverId);

    virtual QString aggregationKey() const override { return m_serverId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo,
        Qn::ResourceInfoLevel detailLevel) const override;

    static ItemDescriptor manifest(common::SystemContext* context);

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QString caption(common::SystemContext* context) const;
    virtual QString name(common::SystemContext* context) const override;
};

} // namespace nx::vms::rules
