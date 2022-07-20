// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API SoftTriggerEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.softTrigger")

    FIELD(QnUuid, triggerId, setTriggerId)
    FIELD(QnUuid, cameraId, setCameraId)
    FIELD(QnUuid, userId, setUserId)
    FIELD(QString, triggerName, setTriggerName)
    FIELD(QString, triggerIcon, setTriggerIcon)

public:
    SoftTriggerEvent() = default;
    SoftTriggerEvent(
        std::chrono::microseconds timestamp,
        State state,
        QnUuid triggerId,
        QnUuid cameraId,
        QnUuid userId,
        const QString& name,
        const QString& icon);

    virtual QString uniqueName() const override;
    virtual QString resourceKey() const override;
    virtual QString aggregationKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString trigger() const;
    QString caption() const;
    QString detailing() const;
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
