// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../data_macros.h"
#include "../event_field.h"

namespace nx::vms::rules {

// TODO: Introduce base class for analytics id fields.

class NX_VMS_RULES_API AnalyticsEventTypeField:
    public EventField,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

    Q_CLASSINFO("metatype", "nx.events.fields.analyticsEventType")

    Q_PROPERTY(QnUuid engineId READ engineId WRITE setEngineId)
    FIELD(QString, typeId, setTypeId)

public:
    AnalyticsEventTypeField(nx::vms::common::SystemContext* context);

    virtual bool match(const QVariant& value) const override;

    QnUuid engineId() const;
    void setEngineId(QnUuid id);

private:
    QnUuid m_engineId;
};

} // namespace nx::vms::rules
