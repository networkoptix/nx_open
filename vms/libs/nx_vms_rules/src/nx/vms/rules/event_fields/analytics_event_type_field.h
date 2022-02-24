// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_field.h"

namespace nx::vms::rules {

// TODO: Introduce base class for analytics id fields.

class NX_VMS_RULES_API AnalyticsEventTypeField: public EventField
{
    Q_OBJECT

    Q_CLASSINFO("metatype", "nx.events.fields.analyticsEventType")

    Q_PROPERTY(QnUuid engineId READ engineId WRITE setEngineId)
    Q_PROPERTY(QnUuid typeId READ typeId WRITE setTypeId)

public:
    virtual bool match(const QVariant& value) const override;

    QnUuid engineId() const;
    void setEngineId(QnUuid id);

    QnUuid typeId() const;
    void setTypeId(QnUuid id);

private:
    QnUuid m_engineId;
    QnUuid m_typeId;
};

} // namespace nx::vms::rules
