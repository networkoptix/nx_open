// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/analytics/taxonomy/abstract_attribute.h>

namespace nx::vms::client::desktop::analytics { class AttributeHelper; }
namespace nx::vms::client::desktop::analytics::taxonomy {

class Attribute;
class AbstractStateViewFilter;

Attribute* mergeNumericAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent);

Attribute* mergeColorTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent);

Attribute* mergeEnumTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent);

Attribute* mergeObjectTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    const AbstractStateViewFilter* filter,
    QObject* parent);

nx::analytics::taxonomy::AbstractAttribute::Type attributeType(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes);

Attribute* wrapAttribute(
    const nx::analytics::taxonomy::AbstractAttribute* taxonomyAttribute,
    QObject* parent);

Attribute* mergeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    const AbstractStateViewFilter* filter,
    QObject* parent);

std::vector<Attribute*> resolveAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractObjectType*>& objectTypes,
    const AbstractStateViewFilter* filter,
    QObject* parent);

/**
 * Returns a filter that matches exact values for enumeration and color attributes.
 */
QString makeEnumValuesExact(
    const QString& filter,
    const AttributeHelper* attributeHelper,
    const QVector<QString>& objectTypeIds);

} // namespace nx::vms::client::desktop::analytics::taxonomy
