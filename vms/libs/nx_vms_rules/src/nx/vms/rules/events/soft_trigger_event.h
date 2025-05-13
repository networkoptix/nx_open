// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API SoftTriggerEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "softTrigger")
    FIELD(nx::Uuid, triggerId, setTriggerId)
    FIELD(nx::Uuid, deviceId, setDeviceId)
    FIELD(nx::Uuid, userId, setUserId)
    FIELD(QString, triggerName, setTriggerName)
    FIELD(QString, triggerIcon, setTriggerIcon)

public:
    SoftTriggerEvent() = default;
    SoftTriggerEvent(
        std::chrono::microseconds timestamp,
        State state,
        nx::Uuid triggerId,
        nx::Uuid deviceId,
        nx::Uuid userId,
        const QString& name,
        const QString& icon);

    virtual QString sequenceKey() const override { return m_deviceId.toSimpleString(); }
    virtual QString aggregationKey() const override { return m_triggerId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

    static const ItemDescriptor& manifest();

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QStringList detailing(common::SystemContext* context, Qn::ResourceInfoLevel detailLevel) const;
};

} // namespace nx::vms::rules
