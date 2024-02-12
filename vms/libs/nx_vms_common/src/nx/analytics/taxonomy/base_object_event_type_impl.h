// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>
#include <nx/analytics/taxonomy/attribute.h>
#include <nx/analytics/taxonomy/attribute_resolver.h>
#include <nx/analytics/taxonomy/color_type.h>
#include <nx/analytics/taxonomy/engine.h>
#include <nx/analytics/taxonomy/entity_type.h>
#include <nx/analytics/taxonomy/enum_type.h>
#include <nx/analytics/taxonomy/error_handler.h>
#include <nx/analytics/taxonomy/event_type.h>
#include <nx/analytics/taxonomy/group.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/analytics/taxonomy/scope.h>

namespace nx::analytics::taxonomy {

template<typename Descriptor, typename AbstractResolvedType, typename ResolvedType>
class BaseObjectEventTypeImpl
{
public:
    BaseObjectEventTypeImpl(
        Descriptor descriptor,
        QString typeName,
        AbstractResourceSupportProxy* resourceSupportProxy,
        ResolvedType* owner)
        :
        m_descriptor(std::move(descriptor)),
        m_owner(owner),
        m_typeName(std::move(typeName)),
        m_resourceSupportProxy(resourceSupportProxy)
    {
        if constexpr (std::is_same_v<Descriptor, nx::vms::api::analytics::EventTypeDescriptor>)
            m_entityType = EntityType::eventType;

        if constexpr (std::is_same_v<Descriptor, nx::vms::api::analytics::ObjectTypeDescriptor>)
            m_entityType = EntityType::objectType;
    }

    QString id() const
    {
        return m_descriptor.id;
    }

    QString name() const
    {
        if (m_descriptor.isSupposedToMimicBaseType() && NX_ASSERT(m_base))
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

        if (m_areSupportedAttributesResolved)
        {
            std::vector<AbstractAttribute*> result;
            for (const AttributeContext& context: m_attributes)
            {
                if (!context.isOwn)
                    result.push_back(context.attribute);
            }

            return result;
        }

        // This code is executed only when compiling the State.
        std::vector<AbstractAttribute*> baseAttributes = m_base->attributes();

        const std::set<QString> omittedBaseAttributeIds{
            m_descriptor.omittedBaseAttributes.cbegin(),
            m_descriptor.omittedBaseAttributes.cend()};

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
        if (m_areSupportedAttributesResolved)
        {
            std::vector<AbstractAttribute*> result;
            for (const AttributeContext& context: m_attributes)
            {
                if (context.isOwn)
                    result.push_back(context.attribute);
            }

            return result;
        }

        return m_ownAttributes;
    }

    std::vector<AbstractAttribute*> attributes() const
    {
        if (m_areSupportedAttributesResolved)
        {
            std::vector<AbstractAttribute*> result;
            for (const AttributeContext& context: m_attributes)
                result.push_back(context.attribute);

            return result;
        }

        // This code is executed only when compiling the State.
        std::map<QString, AbstractAttribute*> ownAttributes;
        for (AbstractAttribute* ownAttribute: m_ownAttributes)
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

        for (AbstractAttribute* ownAttribute: m_ownAttributes)
        {
            if (ownAttributes.find(ownAttribute->name()) != ownAttributes.cend())
                result.push_back(ownAttribute);
        }

        return result;
    }

    std::vector<AbstractAttribute*> supportedAttributes() const
    {
        std::vector<AbstractAttribute*> result;
        for (const AttributeContext& context: m_attributes)
        {
            if (context.attribute->isSupported(/*any Engine*/ nx::Uuid(), /*any Device*/ nx::Uuid()))
                result.push_back(context.attribute);
        }

        return result;
    }

    std::vector<AbstractAttribute*> supportedOwnAttributes() const
    {
        std::vector<AbstractAttribute*> result;
        for (const AttributeContext& context: m_attributes)
        {
            if (context.isOwn && context.attribute->isSupported(
                /*any Engine*/ nx::Uuid(), /*any Device*/ nx::Uuid()))
            {
                result.push_back(context.attribute);
            }
        }

        return result;
    }

    bool hasEverBeenSupported() const
    {
        return m_descriptor.hasEverBeenSupported;
    }

    bool isSupported(nx::Uuid engineId, nx::Uuid deviceId) const
    {
        return m_resourceSupportProxy->isEntityTypeSupported(
            m_entityType, m_descriptor.id, deviceId, engineId);
    }

    bool isReachable() const
    {
        return m_isReachable;
    }

    const std::vector<AbstractScope*>& scopes() const
    {
        return m_scopes;
    }

    Descriptor serialize() const
    {
        return m_descriptor;
    }
    const Descriptor& descriptor() const
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

    void resolveReachability()
    {
        if (isLeaf())
            resolveReachability(/*hasPublicDescendants*/ false);
    }

    void resolveReachability(bool hasPublicDescendants)
    {
        if (m_isReachable)
            return;

        if ((hasPublicDescendants || m_descriptor.hasEverBeenSupported) && !m_descriptor.isHidden())
            m_isReachable = true;

        hasPublicDescendants |= m_isReachable;
        if (m_descriptor.isHidden() && m_descriptor.hasEverBeenSupported)
            hasPublicDescendants = true;

        if (m_base)
            m_base->resolveReachability(hasPublicDescendants);
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
                            m_typeName, m_descriptor.id, scope.groupId)});

                    continue;
                }
            }

            taxonomyScope->setProvider(scope.provider);
            taxonomyScope->setHasTypeEverBeenSupportedInThisScope(scope.hasTypeEverBeenSupportedInThisScope);

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
        if (m_isResolved)
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
            context.baseAttributes = m_base->attributes();

        context.resolvedOwnAttributes = &m_ownAttributes;
        context.owner = m_owner;

        AttributeResolver resolver(context, errorHandler);
        resolver.resolve();

        if (m_base)
            m_base->addDerivedType(m_owner);

        m_isResolved = true;
    }

    void resolveSupportedAttributes(InternalState* inOutInternalState, ErrorHandler* errorHandler)
    {
        // At this point all attributes of all types are resolved but support info is not set yet.
        std::map<QString, AbstractAttribute*> ownAttributes;
        for (AbstractAttribute* ownAttribute: m_ownAttributes)
            ownAttributes[ownAttribute->name()] = ownAttribute;

        for (AbstractAttribute* baseAttribute: this->baseAttributes())
        {
            const auto ownAttributeIt = ownAttributes.find(baseAttribute->name());
            if (ownAttributeIt == ownAttributes.cend())
            {
                m_attributes.push_back({baseAttribute, /*isOwn*/ false});
            }
            else
            {
                m_attributes.push_back({ownAttributeIt->second, /*isOwn*/ true});
                ownAttributes.erase(ownAttributeIt);
            }
        }

        for (AbstractAttribute* ownAttribute: m_ownAttributes)
        {
            if (ownAttributes.find(ownAttribute->name()) != ownAttributes.cend())
                m_attributes.push_back({ownAttribute, /*isOwn*/ true});
        }

        std::vector<AbstractAttribute*> attributes;
        for (AttributeContext& context: m_attributes)
            attributes.push_back(context.attribute);

        attributes = makeSupportedAttributes(
            attributes,
            m_descriptor.attributeSupportInfo,
            m_resourceSupportProxy,
            m_descriptor.id,
            m_entityType);

        NX_ASSERT(attributes.size() == m_attributes.size(),
            "The resolved Attribute list size does not equal the Attribute list size, %1: %2",
            m_typeName, m_descriptor.name);

        for (int i = 0; i < attributes.size(); ++i)
            m_attributes[i].attribute = attributes[i];

        m_areSupportedAttributesResolved = true;
    }

private:
    Descriptor m_descriptor;

    ResolvedType* m_base = nullptr;
    ResolvedType* m_owner = nullptr;
    std::vector<AbstractResolvedType*> m_derivedTypes;
    EntityType m_entityType = EntityType::undefined;

    struct AttributeContext
    {
        AbstractAttribute* attribute;
        bool isOwn = false;
    };

    std::vector<AttributeContext> m_attributes;
    std::vector<AbstractAttribute*> m_ownAttributes;
    std::vector<AbstractScope*> m_scopes;

    QString m_typeName;
    bool m_isReachable = false;
    bool m_isResolved = false;
    bool m_areSupportedAttributesResolved = false;

    AbstractResourceSupportProxy* m_resourceSupportProxy;
};

} // namespace nx::analytics::taxonomy
