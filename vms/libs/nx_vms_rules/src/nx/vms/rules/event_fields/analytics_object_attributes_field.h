// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../data_macros.h"
#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsObjectAttributesField: public EventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.analyticsObjectAttributes")

    FIELD(QString, value, setValue)

public:
    virtual bool match(const QVariant& value) const override;
};

} // namespace nx::vms::rules
