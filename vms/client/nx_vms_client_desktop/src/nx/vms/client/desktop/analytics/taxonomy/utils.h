// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include "abstract_attribute.h"
#include "abstract_state_view_filter.h"

#include <nx/analytics/taxonomy/abstract_attribute.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

AbstractAttribute* mergeNumericAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent);

AbstractAttribute* mergeColorTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent);

AbstractAttribute* mergeEnumTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent);

AbstractAttribute* mergeObjectTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    const AbstractStateViewFilter* filter,
    QObject* parent);

nx::analytics::taxonomy::AbstractAttribute::Type attributeType(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes);

AbstractAttribute::Type fromTaxonomyAttributeType(
    nx::analytics::taxonomy::AbstractAttribute::Type taxonomyAttributeType);

AbstractAttribute* wrapAttribute(
    const nx::analytics::taxonomy::AbstractAttribute* taxonomyAttribute,
    QObject* parent);

AbstractAttribute* mergeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    const AbstractStateViewFilter* filter,
    QObject* parent);

std::vector<AbstractAttribute*> resolveAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractObjectType*>& objectTypes,
    const AbstractStateViewFilter* filter,
    QObject* parent);

} // namespace nx::vms::client::desktop::analytics::taxonomy
