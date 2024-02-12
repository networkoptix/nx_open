// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::api { enum class ResourceStatus; }

namespace nx::vms::rules {

class NX_VMS_RULES_API CameraEvent: public BasicEvent
{
    Q_OBJECT

    FIELD(nx::Uuid, cameraId, setCameraId)

public:
    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

protected:
    CameraEvent() = default;
    CameraEvent(std::chrono::microseconds timestamp, State state, nx::Uuid id);

private:
    nx::vms::api::ResourceStatus sourceStatus(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
