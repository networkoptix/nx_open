// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_object_event_type.h"

#include <nx/analytics/taxonomy/event_type.h>
#include <nx/analytics/taxonomy/object_type.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

template <typename DerivedType, typename DescriptorType>
QString BaseObjectEventType<DerivedType, DescriptorType>::id() const
{
    return m_descriptor.id;
}

template <typename DerivedType, typename DescriptorType>
QString BaseObjectEventType<DerivedType, DescriptorType>::name() const
{
    if (m_descriptor.isSupposedToMimicBaseType() && NX_ASSERT(m_base))
        return m_base->name();

    return m_descriptor.name;
}

template <typename DerivedType, typename DescriptorType>
QString BaseObjectEventType<DerivedType, DescriptorType>::icon() const
{
    if (!m_descriptor.icon.isEmpty())
        return m_descriptor.icon;

    if (m_base)
        return m_base->icon();

    return QString();
}

template <typename DerivedType, typename DescriptorType>
DerivedType* BaseObjectEventType<DerivedType, DescriptorType>::base() const
{
    return m_base;
}

template <typename DerivedType, typename DescriptorType>
std::vector<DerivedType*> BaseObjectEventType<DerivedType, DescriptorType>::derivedTypes() const
{
    return m_derivedTypes;
}

template <typename DerivedType, typename DescriptorType>
std::vector<AbstractAttribute*> BaseObjectEventType<DerivedType, DescriptorType>::baseAttributes() const
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

template <typename DerivedType, typename DescriptorType>
std::vector<AbstractAttribute*> BaseObjectEventType<DerivedType, DescriptorType>::ownAttributes() const
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

template <typename DerivedType, typename DescriptorType>
std::vector<AbstractAttribute*> BaseObjectEventType<DerivedType, DescriptorType>::attributes() const
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

template <typename DerivedType, typename DescriptorType>
std::vector<AbstractAttribute*> BaseObjectEventType<DerivedType, DescriptorType>::supportedAttributes() const
{
    std::vector<AbstractAttribute*> result;
    for (const AttributeContext& context: m_attributes)
    {
        if (context.attribute->isSupported(/*any Engine*/ nx::Uuid(), /*any Device*/ nx::Uuid()))
            result.push_back(context.attribute);
    }

    return result;
}

template <typename DerivedType, typename DescriptorType>
std::vector<AbstractAttribute*> BaseObjectEventType<DerivedType, DescriptorType>::supportedOwnAttributes() const
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

template <typename DerivedType, typename DescriptorType>
bool BaseObjectEventType<DerivedType, DescriptorType>::hasEverBeenSupported() const
{
    return m_descriptor.hasEverBeenSupported;
}

template <typename DerivedType, typename DescriptorType>
bool BaseObjectEventType<DerivedType, DescriptorType>::isSupported(nx::Uuid engineId, nx::Uuid deviceId) const
{
    return m_resourceSupportProxy->isEntityTypeSupported(
        m_entityType, m_descriptor.id, deviceId, engineId);
}

template <typename DerivedType, typename DescriptorType>
bool BaseObjectEventType<DerivedType, DescriptorType>::isReachable() const
{
    return m_isReachable;
}

template <typename DerivedType, typename DescriptorType>
const std::vector<Scope>& BaseObjectEventType<DerivedType, DescriptorType>::scopes() const
{
    return m_scopes;
}

template <typename DerivedType, typename DescriptorType>
const DescriptorType& BaseObjectEventType<DerivedType, DescriptorType>::descriptor() const
{
    return m_descriptor;
}

template <typename DerivedType, typename DescriptorType>
DescriptorType BaseObjectEventType<DerivedType, DescriptorType>::serialize() const
{
    return m_descriptor;
}

template <typename DerivedType, typename DescriptorType>
void BaseObjectEventType<DerivedType, DescriptorType>::addDerivedType(DerivedType* derivedType)
{
    m_derivedTypes.push_back(derivedType);
}

template <typename DerivedType, typename DescriptorType>
bool BaseObjectEventType<DerivedType, DescriptorType>::isLeaf() const
{
    return m_derivedTypes.empty();
}

template <typename DerivedType, typename DescriptorType>
void BaseObjectEventType<DerivedType, DescriptorType>::resolveReachability()
{
    if (isLeaf())
        resolveReachability(/*hasPublicDescendants*/ false);
}

template <typename DerivedType, typename DescriptorType>
void BaseObjectEventType<DerivedType, DescriptorType>::resolveReachability(bool hasPublicDescendants)
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

template <typename DerivedType, typename DescriptorType>
void BaseObjectEventType<DerivedType, DescriptorType>::resolveScopes(
    InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    std::set<DescriptorScope> scopes;

    for (const DescriptorScope& scope : m_descriptor.scopes)
    {
        Scope taxonomyScope;
        if (!scope.engineId.isNull())
        {
            if (Engine* engine =
                inOutInternalState->getTypeById<Engine>(scope.engineId.toString(QUuid::WithBraces)))
            {
                taxonomyScope.setEngine(engine);
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
                taxonomyScope.setGroup(group);
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

        taxonomyScope.setProvider(scope.provider);
        taxonomyScope.setHasTypeEverBeenSupportedInThisScope(scope.hasTypeEverBeenSupportedInThisScope);

        if (!taxonomyScope.isEmpty())
        {
            scopes.insert(scope);
            m_scopes.push_back(std::move(taxonomyScope));
        }
    }

    m_descriptor.scopes = scopes;
}

template <typename DerivedType, typename DescriptorType>
void BaseObjectEventType<DerivedType, DescriptorType>::resolve(InternalState* inOutInternalState,
    ErrorHandler* errorHandler)
{
    if (m_isResolved)
        return;

    if (m_descriptor.base && !m_descriptor.base->isEmpty())
    {
        m_base = inOutInternalState->getTypeById<DerivedType>(*m_descriptor.base);
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
    context.owner = this;

    AttributeResolver resolver(context, errorHandler);
    resolver.resolve();

    if (m_base)
        m_base->addDerivedType(static_cast<DerivedType*>(this));

    m_isResolved = true;
}

template <typename DerivedType, typename DescriptorType>
void BaseObjectEventType<DerivedType, DescriptorType>::resolveSupportedAttributes(
    InternalState* /*inOutInternalState*/, ErrorHandler* /*errorHandler*/)
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

    for (int i = 0; i < (int) attributes.size(); ++i)
        m_attributes[i].attribute = attributes[i];

    m_areSupportedAttributesResolved = true;
}

template class
BaseObjectEventType<EventType, nx::vms::api::analytics::EventTypeDescriptor>;

template class
BaseObjectEventType<ObjectType, nx::vms::api::analytics::ObjectTypeDescriptor>;

} // namespace nx::analytics::taxonomy
