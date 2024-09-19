// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"
#include "../manifest.h"

namespace nx::vms::rules {

struct TargetLayoutsFieldProperties
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

    static TargetLayoutsFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        return TargetLayoutsFieldProperties{
            .visible = properties.value("visible").toBool(),
            .value = properties.value("value").value<UuidSet>(),
            .allowEmptySelection = properties.value("allowEmptySelection").toBool()};
    }
};

/** Stores selected layout ids. Displayed as a layout selection button. */
class NX_VMS_RULES_API TargetLayoutsField:
    public SimpleTypeActionField<UuidSet, TargetLayoutsField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.layouts")

    Q_PROPERTY(UuidSet value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<UuidSet, TargetLayoutsField>::SimpleTypeActionField;

    TargetLayoutsFieldProperties properties() const
    {
        return TargetLayoutsFieldProperties::fromVariantMap(descriptor()->properties);
    }

    static QJsonObject openApiDescriptor(const QVariantMap& properties)
    {
        auto descriptor = SimpleTypeActionField::openApiDescriptor(properties);
        descriptor[utils::kDescriptionProperty] = "Specifies the target layouts for the action.";
        return descriptor;
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
