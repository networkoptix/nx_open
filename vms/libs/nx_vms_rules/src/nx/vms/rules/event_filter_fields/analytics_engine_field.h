// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEngineField:
    public SimpleTypeEventField<nx::Uuid, AnalyticsEngineField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "analyticsEngine")

    Q_PROPERTY(nx::Uuid value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeEventField<nx::Uuid, AnalyticsEngineField>::SimpleTypeEventField;

    virtual bool match(const QVariant& eventValue) const override;
    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
