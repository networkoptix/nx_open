// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../data_macros.h"
#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsObjectTypeField:
    public EventField,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.analyticsObjectType")

    FIELD(QString, value, setValue)

public:
    AnalyticsObjectTypeField(nx::vms::common::SystemContext* context);

    virtual bool match(const QVariant& value) const override;
};

} // namespace nx::vms::rules
