// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/prolonged_event.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API CameraInputEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    CameraInputEvent(const QnResourcePtr& resource, EventState toggleState,
        qint64 timeStamp, const QString& inputPortID);

    const QString& inputPortID() const;

    virtual QString getExternalUniqueKey() const override;
    virtual bool checkEventParams(const EventParameters &params) const override;
    virtual EventParameters getRuntimeParams() const override;

private:
    const QString m_inputPortID;
};

} // namespace event
} // namespace vms
} // namespace nx
