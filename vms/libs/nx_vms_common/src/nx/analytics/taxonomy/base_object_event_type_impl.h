// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/attribute.h>
#include <nx/analytics/taxonomy/proxy_object_type_attribute.h>
#include <nx/analytics/taxonomy/enum_type.h>
#include <nx/analytics/taxonomy/color_type.h>
#include <nx/analytics/taxonomy/scope.h>
#include <nx/analytics/taxonomy/engine.h>
#include <nx/analytics/taxonomy/group.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/analytics/taxonomy/event_type.h>
#include <nx/analytics/taxonomy/error_handler.h>
#include <nx/analytics/taxonomy/attribute_resolver.h>

namespace nx::analytics::taxonomy {

template<typename Descriptor, typename AbstractResolvedType, typename ResolvedType>
class BaseObjectEventTypeImpl
{
public:
    BaseObjectEventTypeImpl(Descriptor descriptor, QString typeName, ResolvedType* owner):
        m_descriptor(std::move(descriptor)),
        m_owner(owner),
        m_typeName(std::move(typeName))
    {
    }

    QString id() const
    {
        return m_descriptor.id;
    }

    QString name() const
    {
        if (m_descriptor.isHidden() && NX_ASSERT(m_base))
            return m_base->name();

        return m_descriptor.name;
    }

    QString icon() const
    {
        if (!m_descriptor.icon.isEmpty())
            return m_descriptor.icon;

        if (m_base)
            return m_base->icon();

        return QString();
    }

    AbstractResolvedType* base() const
    {
        return m_base;
    }

    std::vector<AbstractResolvedType*> derivedTypes() const
    {
        return m_derivedTypes;
    }

    std::vector<AbstractAttribute*> baseAttributes() const
    {
        if (!m_base)
            return {};

        std::vector<AbstractAttribute*> baseAttributes = m_base->attributes();

        const std::set<QString> omittedBaseAttributeIds{
            m_descriptor.omittedBaseAttributes.cbegin(),
            m_descriptor.omittedBaseAttributes.cend() };

        std::vector<AbstractAttribute*> result;

        for (auto& attribute: baseAttributes)
        {
            if (omittedBaseAttributeIds.find(attribute->name()) == omittedBaseAttributeIds.cend())
                result.push_back(attribute);
        }

        return result;
    }

    std::vector<AbstractAttribute*> ownAttributes() const
    {
        return m_internalOwnAttributes;
    }

    std::vector<AbstractAttribute*> attributes() const
    {
        std::map<QString, AbstractAttribute*> ownAttributes;
        for (AbstractAttribute* ownAttribute: m_internalOwnAttributes)
            ownAttributes[ownAttribute->name()] = ownAttribute;

        std::vector<AbstractAttribute*> result;
        for (AbstractAttribute* baseAttribute: this->baseAttributes())
        {
            const auto ownAttributeIt = ownAttributes.find(baseAttribute->name());
            if (ownAttributeIt == ownAttributes.cend())
            {
                result.push_back(baseAttribute);
            }
            else
            {
                result.push_back(ownAttributeIt->second);
                ownAttributes.erase(ownAttributeIt);
            }
        }

        for (AbstractAttribute* ownAttribute: m_internalOwnAttributes)
        {
            if (ownAttributes.find(ownAttribute->name()) != ownAttributes.cend())
                result.push_back(ownAttribute);
        }

        return result;
    }

    std::vector<AbstractAttribute*> supportedAttributes() const
    {
        return m_internalSupportedAttributes;
    }

    std::vector<AbstractAttribute*> supportedOwnAttributes() const
    {
        return m_internalSupportedOwnAttributes;
    }

    bool hasEverBeenSupported() const
    {
        return m_descriptor.hasEverBeenSupported;
    }

    bool isPrivate() const
    {
        return m_isPrivate;
    }

    std::vector<AbstractScope*> scopes() const
    {
        return m_scopes;
    }

    Descriptor serialize() const
    {
        return m_descriptor;
    }

    Descriptor descriptor() const
    {
        return m_descriptor;
    }

    void addDerivedType(AbstractResolvedType* derivedType)
    {
        m_derivedTypes.push_back(derivedType);
    }

    bool isLeaf() const
    {
        return m_derivedTypes.empty();
    }

    void resolvePrivateness(bool hasPublicDescendants)
    {
        if (!m_isPrivate)
            return;

        if ((hasPublicDescendants || m_descriptor.hasEverBeenSupported) && !m_descriptor.isHidden())
            m_isPrivate = false;

        hasPublicDescendants |= !m_isPrivate;
        if (m_descriptor.isHidden() && m_descriptor.hasEverBeenSupported)
            hasPublicDescendants = true;

        if (m_base)
            m_base->resolvePrivateness(hasPublicDescendants);
    }

    void resolveScopes(InternalState* inOutInternalState, ErrorHandler* errorHandler)
    {
        std::set<nx::vms::api::analytics::DescriptorScope> scopes;

        for (const nx::vms::api::analytics::DescriptorScope& scope : m_descriptor.scopes)
        {
            auto taxonomyScope = new Scope(m_owner);
            if (!scope.engineId.isNull())
            {
                if (Engine* engine =
                    inOutInternalState->getTypeById<Engine>(scope.engineId.toString()); engine)
                {
                    taxonomyScope->setEngine(engine);
                }
                else
                {
                    errorHandler->handleError(
                        ProcessingError{NX_FMT(
                            "%1 %2: Unable to find a descriptor of "
                            "Engine %3 used in a scope",
                            m_typeName, m_descriptor.id, scope.engineId)});

                    continue;
                }
            }

            if (!scope.groupId.isEmpty())
            {
                if (Group* group = inOutInternalState->getTypeById<Group>(scope.groupId);
                    group)
                {
                    taxonomyScope->setGroup(group);
                }
                else
                {
                    errorHandler->handleError(
                        ProcessingError{NX_FMT(
                            "%1 %2: unable to find a descriptor "
                            "of Group %3 used in a scope",
                            m_typeName, m_descriptor.id, scope.groupId) });

                    continue;
                }
            }

            if (!taxonomyScope->isEmpty())
            {
                scopes.insert(scope);
                m_scopes.push_back(taxonomyScope);
            }
        }

        m_descriptor.scopes = scopes;
    }

    void resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler)
    {
        if (m_resolved)
            return;

        if (m_descriptor.base && !m_descriptor.base->isEmpty())
        {
            m_base = inOutInternalState->getTypeById<ResolvedType>(*m_descriptor.base);
            if (NX_ASSERT(m_base, "%1 %2: unable to find base (%3)",
                m_typeName, m_descriptor.id, *m_descriptor.base))
            {
                m_base->resolve(inOutInternalState, errorHandler);
            }
        }

        resolveScopes(inOutInternalState, errorHandler);

        AttributeResolver::Context context;
        context.internalState = inOutInternalState;
        context.typeName = m_typeName;
        context.typeId = m_descriptor.id;
        context.baseTypeId = m_base ? m_base->id() : QString();
        context.ownAttributes = &m_descriptor.attributes;
        context.omittedBaseAttributeNames = &m_descriptor.omittedBaseAttributes;
        if (m_base)
            context.baseAttributes = m_base->baseAttributes();

        context.resolvedOwnAttributes = &m_internalOwnAttributes;
        context.owner = m_owner;

        AttributeResolver resolver(context, errorHandler);
        resolver.resolve();

        if (m_base)
            m_base->addDerivedType(m_owner);

        m_resolved = true;
    }

    void resolveSupportedAttributes(InternalState* inOutInternalState, ErrorHandler* errorHandler)
    {
        // TODO: handle errors.
        // TODO: Either rename this method or resolve privateness in another place.

        if (isLeaf())
            resolvePrivateness(/*hasPublicDescendants*/ false);

        std::set<QString> attributePaths;
        for (const auto& [attributePath, _]: m_descriptor.attributeSupportInfo)
            attributePaths.insert(attributePath);

        const AttributeTree attributeTree = getSupportedAttributeTree(
            attributePaths, inOutInternalState);

        m_internalSupportedAttributes = makeSupportedAttributes(this->attributes(), attributeTree);
        m_internalSupportedOwnAttributes = makeSupportedAttributes(
            this->ownAttributes(), attributeTree);
    }

    AttributeTree getSupportedAttributeTree(
        std::set<QString> attributePaths, InternalState* internalState) const
    {
        AttributeTree result;
        std::vector<const AbstractAttribute*> nestedAttributes;

        for (const AbstractAttribute* attribute: this->attributes())
        {
            if (attributePaths.empty())
                return result;

            if (attribute->type() == AbstractAttribute::Type::object)
                nestedAttributes.push_back(attribute);

            const QString attributeName = attribute->name();
            if (contains(attributePaths, attributeName))
            {
                result.children.emplace(attributeName, AttributeTree());
                attributePaths.erase(attributeName);
            }
        }

        for (const AbstractAttribute* attribute: nestedAttributes)
        {
            if (attributePaths.empty())
                return result;

            const ObjectType* objectType =
                internalState->getTypeById<ObjectType>(attribute->subtype());

            if (!NX_ASSERT(objectType))
                continue;

            const QString attributeName = attribute->name();
            const QString prefix = attributeName + ".";

            std::set<QString> nestedAttributePaths;
            for (auto itr = attributePaths.lower_bound(prefix);
                itr != attributePaths.cend() && itr->startsWith(prefix);
                ++itr)
            {
                QString attributeSuffix = itr->mid(prefix.length());
                if (!attributeSuffix.isEmpty())
                    nestedAttributePaths.insert(std::move(attributeSuffix));
            }

            if (nestedAttributePaths.empty())
                continue;

            result.children[attributeName] =
                objectType->getSupportedAttributeTree(nestedAttributePaths, internalState);

            nestedAttributePaths.clear();
            pathsFromTree(&nestedAttributePaths, result.children[attributeName], prefix);
            for (const QString& path: nestedAttributePaths)
                attributePaths.erase(path);
        }

        return result;
    }

private:
    Descriptor m_descriptor;

    ResolvedType* m_base = nullptr;
    ResolvedType* m_owner = nullptr;
    std::vector<AbstractResolvedType*> m_derivedTypes;
    std::vector<AbstractAttribute*> m_internalOwnAttributes;
    std::vector<AbstractAttribute*> m_internalSupportedAttributes;
    std::vector<AbstractAttribute*> m_internalSupportedOwnAttributes;

    std::vector<AbstractScope*> m_scopes;

    QString m_typeName;
    bool m_isPrivate = true;
    bool m_resolved = false;
};

} // namespace nx::analytics::taxonomy
