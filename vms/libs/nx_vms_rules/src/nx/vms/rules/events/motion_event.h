// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../data_macros.h"
#include "camera_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API MotionEvent: public CameraEvent
{
    Q_OBJECT
    using base_type = CameraEvent;
    Q_CLASSINFO("type", "nx.events.motion")

public:
    MotionEvent() = default;
    MotionEvent(std::chrono::microseconds timestamp, State state, nx::Uuid deviceId);

    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
