// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores single layout. Displayed as a layout selection button. */
class NX_VMS_RULES_API TargetLayoutField: public SimpleTypeActionField<nx::Uuid, TargetLayoutField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.layout")

    Q_PROPERTY(nx::Uuid value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<nx::Uuid, TargetLayoutField>::SimpleTypeActionField;
    static QJsonObject openApiDescriptor(const QVariantMap& properties)
    {
        auto descriptor = SimpleTypeActionField::openApiDescriptor(properties);
        descriptor[utils::kDescriptionProperty] = "Specifies the target layout for the action.";
        return descriptor;
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
