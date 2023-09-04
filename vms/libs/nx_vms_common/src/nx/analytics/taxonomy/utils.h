// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>

#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/analytics/taxonomy/entity_type.h>

namespace nx::analytics::taxonomy {

class AbstractEventType;
class AbstractResourceSupportProxy;

template<typename Container, typename Item>
bool contains(const Container& container, const Item& item)
{
    return container.find(item) != container.cend();
}

struct AttributeSupportInfoTree
{
    std::set<QnUuid> supportByEngine;
    std::map<QString /*attributeName*/, AttributeSupportInfoTree> nestedAttributeSupportInfo;
};

std::map<QString, AttributeSupportInfoTree> buildAttributeSupportInfoTree(
    const std::vector<AbstractAttribute*>& attributes,
    std::map<QString, std::set<QnUuid /*engineId*/>> supportInfo);

std::vector<AbstractAttribute*> makeSupportedAttributes(
    const std::vector<AbstractAttribute*>& attributes,
    std::map<QString, std::set<QnUuid /*engineId*/>> supportInfo,
    AbstractResourceSupportProxy* resourceSupportProxy,
    QString rootParentTypeId,
    EntityType rootEntityType);

AbstractAttribute::Type fromDescriptorAttributeType(
    nx::vms::api::analytics::AttributeType attributeType);

nx::vms::api::analytics::AttributeType toDescriptorAttributeType(
    AbstractAttribute::Type attributeType);

NX_VMS_COMMON_API bool eventBelongsToGroup(
    const AbstractEventType* eventType,
    const QString& groupId);

QString maybeUnscopedExtendedObjectTypeId(const QString& scopedExtendedObjectTypeId);

} // namespace nx::analytics::taxonomy
