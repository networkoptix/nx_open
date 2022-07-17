// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/prolonged_event.h>

class QnUuid;

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API SoftwareTriggerEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    SoftwareTriggerEvent(
        const QnResourcePtr& resource,
        const QString& triggerId,
        const QnUuid& userId,
        qint64 timeStampUs,
        EventState toggleState,
        const QString& triggerName);

    const QString& triggerId() const;
    const QnUuid& userId() const;
    const QString& triggerName() const;

    virtual EventParameters getRuntimeParams() const override;
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

private:
    const QString m_triggerId;
    const QnUuid m_userId;
    const QString m_triggerName;
};

} // namespace event
} // namespace vms
} // namespace nx
