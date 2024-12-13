// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_layouts_field.h"

namespace nx::vms::rules {

QVariantMap TargetLayoutsFieldProperties::toVariantMap() const
{
    QVariantMap result;

    result.insert("value", QVariant::fromValue(value));
    result.insert("visible", visible);
    result.insert("allowEmptySelection", allowEmptySelection);

    return result;
}

TargetLayoutsFieldProperties TargetLayoutsFieldProperties::fromVariantMap(
    const QVariantMap& properties)
{
    return TargetLayoutsFieldProperties{
        .visible = properties.value("visible").toBool(),
        .value = properties.value("value").value<UuidSet>(),
        .allowEmptySelection = properties.value("allowEmptySelection").toBool()};
}

TargetLayoutsFieldProperties TargetLayoutsField::properties() const
{
    return TargetLayoutsFieldProperties::fromVariantMap(descriptor()->properties);
}

QJsonObject TargetLayoutsField::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor = SimpleTypeActionField::openApiDescriptor(properties);
    descriptor[utils::kDescriptionProperty] = "Specifies the target layouts for the action.";
    return descriptor;
}

QVariant TargetLayoutsField::build(const AggregatedEventPtr & event) const
{
    // Resource extraction code expects nx::Uuid or UuidList types.
    return QVariant::fromValue(value().values());
}

} // namespace nx::vms::rules
