// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"
#include "../manifest.h"

namespace nx::vms::rules {

struct TargetLayoutFieldProperties
{
    /** Whether given field should be visible in the editor. */
    bool visible{true};

    /** Value for the the just created field's `value` property. */
    UuidSet value;

    bool allowEmptySelection{false};

    QVariantMap toVariantMap() const
    {
        QVariantMap result;

        result.insert("value", QVariant::fromValue(value));
        result.insert("visible", visible);
        result.insert("allowEmptySelection", allowEmptySelection);

        return result;
    }

    static TargetLayoutFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        TargetLayoutFieldProperties result;

        result.value = properties.value("value").value<UuidSet>();
        result.visible = properties.value("visible").toBool();
        result.allowEmptySelection = properties.value("allowEmptySelection").toBool();

        return result;
    }
};

/** Stores selected layout ids. Displayed as a layout selection button. */
class NX_VMS_RULES_API TargetLayoutField: public SimpleTypeActionField<UuidSet, TargetLayoutField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.layouts")

    Q_PROPERTY(UuidSet value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<UuidSet, TargetLayoutField>::SimpleTypeActionField;

    TargetLayoutFieldProperties properties() const
    {
        return TargetLayoutFieldProperties::fromVariantMap(descriptor()->properties);
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
