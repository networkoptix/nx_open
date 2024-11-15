// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ServerStartedEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "serverStarted")
    using base_type = BasicEvent;

    FIELD(nx::Uuid, serverId, setServerId)

public:
    static const ItemDescriptor& manifest();

    ServerStartedEvent() = default;
    ServerStartedEvent(std::chrono::microseconds timestamp, nx::Uuid serverId);

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

private:
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
