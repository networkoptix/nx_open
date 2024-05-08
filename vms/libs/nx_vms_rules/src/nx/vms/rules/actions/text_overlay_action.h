// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API TextOverlayAction: public BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.textOverlay")
    using base_type = BasicAction;

    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(nx::vms::rules::UuidSelection, users, setUsers)
    FIELD(std::chrono::microseconds, duration, setDuration)
    FIELD(QString, text, setText)

    FIELD(QString, extendedCaption, setExtendedCaption)
    FIELD(QStringList, detailing, setDetailing)

public:
    static const ItemDescriptor& manifest();

    virtual QVariantMap details(common::SystemContext* context) const override;
};

} // namespace nx::vms::rules
