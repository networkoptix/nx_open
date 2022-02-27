// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/taxonomy/abstract_attribute.h>

namespace nx::analytics::taxonomy {

template<typename Container, typename Item>
bool contains(const Container& container, const Item& item)
{
    return container.find(item) != container.cend();
}

AbstractAttribute::Type fromDescriptorAttributeType(
    nx::vms::api::analytics::AttributeType attributeType);

nx::vms::api::analytics::AttributeType toDescriptorAttributeType(
    AbstractAttribute::Type attributeType);

struct AttributeTree
{
    std::map<QString, AttributeTree> children;
};

void pathsFromTree(
    std::set<QString>* inOutPaths,
    const AttributeTree& attributeTree,
    const QString& prefix = QString());

std::vector<AbstractAttribute*> makeSupportedAttributes(
    const std::vector<AbstractAttribute*>& allAttributes,
    const AttributeTree& supportedAttributeTree);

} // namespace nx::analytics::taxonomy
