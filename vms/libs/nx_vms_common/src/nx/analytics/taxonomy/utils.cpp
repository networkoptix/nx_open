// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <optional>

#include "abstract_event_type.h"
#include "abstract_group.h"
#include "abstract_object_type.h"
#include "abstract_scope.h"
#include "abstract_state.h"
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

std::set<QnUuid> propagateOwnSupport(AttributeSupportInfoTree* inOutAttributeSupportInfoTree)
{
    if (inOutAttributeSupportInfoTree->nestedAttributeSupportInfo.empty())
        return inOutAttributeSupportInfoTree->supportByEngine;

    for (auto& [nestedAttributeName, nestedAttributeSupportInfoTree]:
        inOutAttributeSupportInfoTree->nestedAttributeSupportInfo)
    {
        const std::set<QnUuid> descendantSupportByEngine =
            propagateOwnSupport(&nestedAttributeSupportInfoTree);

        inOutAttributeSupportInfoTree->supportByEngine.insert(
            descendantSupportByEngine.begin(), descendantSupportByEngine.end());
    }

    return inOutAttributeSupportInfoTree->supportByEngine;
}

std::map<QString, AttributeSupportInfoTree> buildAttributeSupportInfoTree(
    const std::vector<AbstractAttribute*>& attributes,
    std::map<QString, std::set<QnUuid /*engineId*/>> supportInfo)
{
    std::map<QString, AttributeSupportInfoTree> result;

    std::vector<AbstractAttribute*> objectTypeAttributes;
    for (AbstractAttribute* attribute: attributes)
    {
        const QString& attributeName = attribute->name();
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
        const AbstractObjectType* objectType = objectTypeAttribute->objectType();
        if (!NX_ASSERT(objectType))
            continue;

        const QString prefix = objectTypeAttribute->name() + ".";
        auto lowerBoundIt = supportInfo.lower_bound(prefix);

        std::map<QString, std::set<QnUuid>> nestedSupportInfo;
        while (lowerBoundIt != supportInfo.cend() && lowerBoundIt->first.startsWith(prefix))
        {
            nestedSupportInfo[lowerBoundIt->first.mid(prefix.length())] =
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
    std::map<QString, std::set<QnUuid /*engineId*/>> supportInfo,
    AbstractResourceSupportProxy* resourceSupportProxy,
    QString rootParentTypeId,
    EntityType rootEntityType)
{
    std::map<QString, AttributeSupportInfoTree> supportInfoTree =
        buildAttributeSupportInfoTree(attributes, std::move(supportInfo));

    std::vector<AbstractAttribute*> result;
    for (AbstractAttribute* attribute: attributes)
    {
        result.push_back(new ProxyAttribute(
            attribute,
            std::move(supportInfoTree[attribute->name()]),
            resourceSupportProxy,
            /*prefix*/ QString(),
            rootParentTypeId,
            rootEntityType));
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

QList<QString> getAttributesNames(const AbstractState* taxonomyState, const QString& objectId)
{
    QList<QString> result;
    std::function<void(const AbstractObjectType*, QString)> addAttributesRecursive =
        [&](const AbstractObjectType* objectType, const QString& prefix)
        {
            if (!objectType)
                return;

            for (const auto& attribute: objectType->attributes())
            {
                const QString attributeName =
                    prefix.isEmpty() ? attribute->name() : prefix + "." + attribute->name();

                if (attribute->type() == AbstractAttribute::Type::object)
                    addAttributesRecursive(attribute->objectType(), attributeName);
                else
                    result.push_back(attributeName);
            }
        };

    addAttributesRecursive(taxonomyState->objectTypeById(objectId), {});
    return result;
}

// TODO: Remove later, when the code debt issue described below is fixed.
//
// The problem should be solved differently: the notion of "Extended Object Type" must be fully
// evaporated in the phase of compiling the taxonomy. Currently there is a tech debt which
// prevents this - the manifests are accessed by the Client directly through Resource Properties,
// which should be fixed in the first place.
QString maybeUnscopedExtendedObjectTypeId(const QString& scopedExtendedObjectTypeId)
{
    if (!scopedExtendedObjectTypeId.contains("$"))
        return scopedExtendedObjectTypeId;

    QStringList parts = scopedExtendedObjectTypeId.split("$");
    return parts.last();
}

} // namespace nx::analytics::taxonomy
