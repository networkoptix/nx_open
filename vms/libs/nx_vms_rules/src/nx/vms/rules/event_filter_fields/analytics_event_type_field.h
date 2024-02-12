// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../event_filter_field.h"

namespace nx::vms::rules {

// TODO: Introduce base class for analytics id fields.

class NX_VMS_RULES_API AnalyticsEventTypeField:
    public EventFilterField,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

    Q_CLASSINFO("metatype", "nx.events.fields.analyticsEventType")

    Q_PROPERTY(nx::Uuid engineId READ engineId WRITE setEngineId NOTIFY engineIdChanged)
    Q_PROPERTY(QString typeId READ typeId WRITE setTypeId NOTIFY typeIdChanged)

public:
    AnalyticsEventTypeField(nx::vms::common::SystemContext* context);

    virtual bool match(const QVariant& value) const override;

    nx::Uuid engineId() const;
    void setEngineId(nx::Uuid id);

    QString typeId() const;
    void setTypeId(const QString& id);

signals:
    void engineIdChanged();
    void typeIdChanged();

private:
    nx::Uuid m_engineId;
    QString m_typeId;
};

} // namespace nx::vms::rules
