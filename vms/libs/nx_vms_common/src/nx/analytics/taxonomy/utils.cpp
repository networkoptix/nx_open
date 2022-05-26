// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include "abstract_event_type.h"
#include "abstract_group.h"
#include "abstract_scope.h"
#include "proxy_object_type_attribute.h"

namespace nx::analytics::taxonomy {

using namespace nx::vms::api::analytics;

AbstractAttribute::Type fromDescriptorAttributeType(AttributeType attributeType)
{
    switch (attributeType)
    {
        case AttributeType::number: return AbstractAttribute::Type::number;
        case AttributeType::boolean: return AbstractAttribute::Type::boolean;
        case AttributeType::string: return AbstractAttribute::Type::string;
        case AttributeType::color: return AbstractAttribute::Type::color;
        case AttributeType::enumeration: return AbstractAttribute::Type::enumeration;
        case AttributeType::object: return AbstractAttribute::Type::object;
        default:
            NX_ASSERT(false, "Unknown attribute type %1", (int)attributeType);
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

void pathsFromTree(
    std::set<QString>* inOutPaths,
    const AttributeTree& attributeTree,
    const QString& prefix)
{
    for (const auto& [attributeName, subTree]: attributeTree.children)
    {
        if (subTree.children.empty())
            inOutPaths->insert(prefix + attributeName);
        else
            pathsFromTree(inOutPaths, subTree, prefix + attributeName + ".");
    }
}

std::vector<AbstractAttribute*> makeSupportedAttributes(
    const std::vector<AbstractAttribute*>& allAttributes,
    const AttributeTree& supportedAttributeTree)
{
    std::vector<AbstractAttribute*> result;

    for (AbstractAttribute* attribute: allAttributes)
    {
        const QString attributeName = attribute->name();
        if (!contains(supportedAttributeTree.children, attributeName))
            continue;

        if (attribute->type() != AbstractAttribute::Type::object)
        {
            result.push_back(attribute);
        }
        else
        {
            result.push_back(
                new ProxyObjectTypeAttribute(
                    attribute,
                    supportedAttributeTree.children.at(attributeName)));
        }
    }

    return result;
}

bool eventBelongsToGroup(const AbstractEventType* eventType, const QString& groupId)
{
    for (const AbstractScope* scope: eventType->scopes())
    {
        const AbstractGroup* group = scope->group();
        if (!group)
            continue;

        if (group->id() == groupId)
            return true;
    }

    return false;
};

} // namespace nx::analytics::taxonomy
