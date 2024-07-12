// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API DeviceDisconnectedEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.deviceDisconnected")
    using base_type = BasicEvent;

    FIELD(nx::Uuid, cameraId, setCameraId)

public:
    DeviceDisconnectedEvent() = default;
    DeviceDisconnectedEvent(std::chrono::microseconds timestamp, nx::Uuid deviceId);

    virtual QString uniqueName() const override;
    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

    static ItemDescriptor manifest(common::SystemContext* context);

private:
    QString caption(common::SystemContext* context) const;
    QString extendedCaption(common::SystemContext* context) const;
    static QString name(common::SystemContext* context);
};

} // namespace nx::vms::rules
