// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <set>

#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/analytics/taxonomy/entity_type.h>

namespace nx::analytics::taxonomy {

class EventType;
class AbstractResourceSupportProxy;
class AbstractState;

template<typename Container, typename Item>
bool contains(const Container& container, const Item& item)
{
    return container.find(item) != container.cend();
}

struct AttributeSupportInfoTree
{
    std::set<nx::Uuid> supportByEngine;
    std::map<std::string /*attributeName*/, AttributeSupportInfoTree> nestedAttributeSupportInfo;
};

std::vector<AbstractAttribute*> makeSupportedAttributes(
    const std::vector<AbstractAttribute*>& attributes,
    std::map<std::string, std::set<nx::Uuid /*engineId*/>> supportInfo,
    AbstractResourceSupportProxy* resourceSupportProxy,
    const std::string& rootParentTypeId,
    EntityType rootEntityType);

AbstractAttribute::Type fromDescriptorAttributeType(
    std::optional<nx::vms::api::analytics::AttributeType> attributeType);

nx::vms::api::analytics::AttributeType toDescriptorAttributeType(
    AbstractAttribute::Type attributeType);

NX_VMS_COMMON_API bool eventBelongsToGroup(
    const EventType* eventType,
    const std::string& groupId);

std::string maybeUnscopedExtendedObjectTypeId(const std::string& scopedExtendedObjectTypeId);

NX_VMS_COMMON_API std::vector<std::string> getAttributesNames(
    const AbstractState* taxonomyState, const std::string& objectId);

NX_VMS_COMMON_API AbstractAttribute* findAttributeByName(
    const AbstractState* taxonomyState, const std::string& objectTypeId, const std::string& name);

} // namespace nx::analytics::taxonomy
