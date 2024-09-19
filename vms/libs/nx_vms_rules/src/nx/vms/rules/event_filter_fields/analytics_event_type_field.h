// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../event_filter_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEventTypeField:
    public EventFilterField,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

    Q_CLASSINFO("metatype", "nx.events.fields.analyticsEventType")

    Q_PROPERTY(QString typeId READ typeId WRITE setTypeId NOTIFY typeIdChanged)

public:
    AnalyticsEventTypeField(common::SystemContext* context, const FieldDescriptor* descriptor);

    virtual bool match(const QVariant& value) const override;

    QString typeId() const;
    void setTypeId(const QString& id);

    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void typeIdChanged();

private:
    QString m_typeId;
};

} // namespace nx::vms::rules
