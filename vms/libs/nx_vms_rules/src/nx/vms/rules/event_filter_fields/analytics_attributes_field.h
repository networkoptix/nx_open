// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsAttributesField:
    public SimpleTypeEventField<QString, AnalyticsAttributesField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "analyticsAttributes")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeEventField<QString, AnalyticsAttributesField>::SimpleTypeEventField;

    virtual bool match(const QVariant& eventValue) const override;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
