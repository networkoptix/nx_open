// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsObjectTypeField:
    public SimpleTypeEventField<QString, AnalyticsObjectTypeField>,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.analyticsObjectType")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    AnalyticsObjectTypeField(common::SystemContext* context, const FieldDescriptor* descriptor);

    virtual bool match(const QVariant& eventValue) const override;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
