// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API MotionEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "motion")
    FIELD(nx::Uuid, deviceId, setDeviceId)

public:
    MotionEvent() = default;
    MotionEvent(std::chrono::microseconds timestamp, State state, nx::Uuid deviceId);

    virtual QString aggregationKey() const override { return m_deviceId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

    static const ItemDescriptor& manifest();

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;
};

} // namespace nx::vms::rules
