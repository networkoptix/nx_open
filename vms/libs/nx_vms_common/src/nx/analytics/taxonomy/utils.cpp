// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <optional>

#include <nx/utils/std/algorithm.h>

#include "abstract_group.h"
#include "abstract_state.h"
#include "event_type.h"
#include "object_type.h"
#include "proxy_attribute.h"

namespace nx::analytics::taxonomy {

using namespace nx::vms::api::analytics;

AbstractAttribute::Type fromDescriptorAttributeType(std::optional<nx::vms::api::analytics::AttributeType> attributeType)
{
    if (attributeType == std::nullopt)
        return AbstractAttribute::Type::undefined;

    switch (const auto value = attributeType.value())
    {
        case AttributeType::number: return AbstractAttribute::Type::number;
        case AttributeType::boolean: return AbstractAttribute::Type::boolean;
        case AttributeType::string: return AbstractAttribute::Type::string;
        case AttributeType::color: return AbstractAttribute::Type::color;
        case AttributeType::enumeration: return AbstractAttribute::Type::enumeration;
        case AttributeType::object: return AbstractAttribute::Type::object;
        default:
            NX_ASSERT(false, "Unknown Attribute type %1", (int) value);
            return AbstractAttribute::Type::undefined;
    }
}

AttributeType toDescriptorAttributeType(AbstractAttribute::Type attributeType)
{
    switch (attributeType)
    {
        case AbstractAttribute::Type::number: return AttributeType::number;
        case AbstractAttribute::Type::boolean: return AttributeType::boolean;
        case AbstractAttribute::Type::string: return AttributeType::string;
        case AbstractAttribute::Type::color: return AttributeType::color;
        case AbstractAttribute::Type::enumeration: return AttributeType::enumeration;
        case AbstractAttribute::Type::object: return AttributeType::object;
        default:
            NX_ASSERT(false, "Unknown attribute type %1", int (attributeType));
            return AttributeType::undefined;
    }
}

std::set<nx::Uuid> propagateOwnSupport(AttributeSupportInfoTree* inOutAttributeSupportInfoTree)
{
    if (inOutAttributeSupportInfoTree->nestedAttributeSupportInfo.empty())
        return inOutAttributeSupportInfoTree->supportByEngine;

    for (auto& [nestedAttributeName, nestedAttributeSupportInfoTree]:
        inOutAttributeSupportInfoTree->nestedAttributeSupportInfo)
    {
        const std::set<nx::Uuid> descendantSupportByEngine =
            propagateOwnSupport(&nestedAttributeSupportInfoTree);

        inOutAttributeSupportInfoTree->supportByEngine.insert(
            descendantSupportByEngine.begin(), descendantSupportByEngine.end());
    }

    return inOutAttributeSupportInfoTree->supportByEngine;
}

std::map<std::string, AttributeSupportInfoTree> buildAttributeSupportInfoTree(
    const std::vector<AbstractAttribute*>& attributes,
    std::map<std::string, std::set<nx::Uuid /*engineId*/>> supportInfo)
{
    std::map<std::string, AttributeSupportInfoTree> result;

    std::vector<AbstractAttribute*> objectTypeAttributes;
    for (AbstractAttribute* attribute: attributes)
    {
        const std::string& attributeName = attribute->name();
        if (auto it = supportInfo.find(attributeName); it != supportInfo.cend())
        {
            result[attributeName].supportByEngine = std::move(it->second);
            supportInfo.erase(it);
        }

        if (attribute->type() == AbstractAttribute::Type::object)
        {
            objectTypeAttributes.push_back(attribute);
        }
    }

    for (const AbstractAttribute* objectTypeAttribute: objectTypeAttributes)
    {
        const ObjectType* objectType = objectTypeAttribute->objectType();
        if (!NX_ASSERT(objectType))
            continue;

        const std::string prefix = objectTypeAttribute->name() + ".";
        auto lowerBoundIt = supportInfo.lower_bound(prefix);

        std::map<std::string, std::set<nx::Uuid>> nestedSupportInfo;
        while (lowerBoundIt != supportInfo.cend() && lowerBoundIt->first.starts_with(prefix))
        {
            nestedSupportInfo[lowerBoundIt->first.substr(prefix.length())] =
                supportInfo[lowerBoundIt->first];
            ++lowerBoundIt;
        }

        if (nestedSupportInfo.empty())
            continue;

        result[objectTypeAttribute->name()].nestedAttributeSupportInfo =
            buildAttributeSupportInfoTree(objectType->attributes(), nestedSupportInfo);
    }

    for (auto& [_, attributeSupportInfoTree]: result)
        propagateOwnSupport(&attributeSupportInfoTree);

    return result;
}

std::vector<AbstractAttribute*> makeSupportedAttributes(
    const std::vector<AbstractAttribute*>& attributes,
    std::map<std::string, std::set<nx::Uuid /*engineId*/>> supportInfo,
    AbstractResourceSupportProxy* resourceSupportProxy,
    const std::string& rootParentTypeId,
    EntityType rootEntityType)
{
    std::map<std::string, AttributeSupportInfoTree> supportInfoTree =
        buildAttributeSupportInfoTree(attributes, std::move(supportInfo));

    std::vector<AbstractAttribute*> result;
    for (AbstractAttribute* attribute: attributes)
    {
        result.push_back(new ProxyAttribute(
            attribute,
            std::move(supportInfoTree[attribute->name()]),
            resourceSupportProxy,
            /*prefix*/ std::string(),
            rootParentTypeId,
            rootEntityType));
    }

    return result;
}

bool eventBelongsToGroup(const EventType* eventType, const std::string& groupId)
{
    for (const auto& scope: eventType->scopes())
    {
        const AbstractGroup* group = scope.group();
        if (!group)
            continue;

        if (group->id() == groupId)
            return true;
    }

    return false;
};

std::vector<std::string> getAttributesNames(const AbstractState* taxonomyState, const std::string& objectId)
{
    std::vector<std::string> result;
    std::function<void(const ObjectType*, std::string)> addAttributesRecursive =
        [&](const ObjectType* objectType, const std::string& prefix)
        {
            if (!objectType)
                return;

            for (const auto& attribute: objectType->attributes())
            {
                const std::string attributeName =
                    prefix.empty() ? attribute->name() : prefix + "." + attribute->name();

                result.push_back(attributeName);

                if (attribute->type() == AbstractAttribute::Type::object)
                    addAttributesRecursive(attribute->objectType(), attributeName);
            }
        };

    addAttributesRecursive(taxonomyState->objectTypeById(objectId), {});
    return result;
}

NX_VMS_COMMON_API AbstractAttribute* findAttributeByName(
    const AbstractState* taxonomyState, const std::string& objectTypeId, const std::string& name)
{
    auto findAttribute = nx::utils::y_combinator(
        [](auto findAttribute, const ObjectType* objectType, const std::string& name)
            -> AbstractAttribute*
        {
            if (!objectType)
                return nullptr;

            for (AbstractAttribute* attribute: objectType->attributes())
            {
                const std::string& attributeName = attribute->name();

                if (attributeName == name)
                    return attribute;

                if (attribute->type() == AbstractAttribute::Type::object
                    && name.starts_with(attributeName)
                    && name.size() > attributeName.size()
                    && name[attributeName.size()] == '.')
                {
                    AbstractAttribute* result = findAttribute(
                        attribute->objectType(), name.substr(attributeName.size() + 1));

                    if (result)
                        return result;
                }
            }

            return nullptr;
        }
    );

    return findAttribute(taxonomyState->objectTypeById(objectTypeId), name);
}

// TODO: Remove later, when the code debt issue described below is fixed.
//
// The problem should be solved differently: the notion of "Extended Object Type" must be fully
// evaporated in the phase of compiling the taxonomy. Currently there is a tech debt which
// prevents this - the manifests are accessed by the Client directly through Resource Properties,
// which should be fixed in the first place.
std::string maybeUnscopedExtendedObjectTypeId(const std::string& scopedExtendedObjectTypeId)
{
    const auto pos = scopedExtendedObjectTypeId.rfind('$');
    return pos == std::string::npos
        ? scopedExtendedObjectTypeId
        : scopedExtendedObjectTypeId.substr(pos + 1);
}

} // namespace nx::analytics::taxonomy
