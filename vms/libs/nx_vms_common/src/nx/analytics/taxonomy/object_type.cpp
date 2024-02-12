// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_type.h"

#include <nx/analytics/taxonomy/base_object_event_type_impl.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

ObjectType::ObjectType(
    ObjectTypeDescriptor objectTypeDescriptor,
    AbstractResourceSupportProxy* resourceSupportProxy,
    QObject* parent)
    :
    AbstractObjectType(parent),
    m_impl(std::make_shared<
        BaseObjectEventTypeImpl<ObjectTypeDescriptor, AbstractObjectType, ObjectType>>(
            objectTypeDescriptor,
            kObjectTypeDescriptorTypeName,
            resourceSupportProxy,
            /*owner*/ this))
{
}

QString ObjectType::id() const
{
    return m_impl->id();
}

QString ObjectType::name() const
{
    return m_impl->name();
}

QString ObjectType::icon() const
{
    return m_impl->icon();
}

AbstractObjectType* ObjectType::base() const
{
    return m_impl->base();
}

std::vector<AbstractObjectType*> ObjectType::derivedTypes() const
{
    return m_impl->derivedTypes();
}

std::vector<AbstractAttribute*> ObjectType::baseAttributes() const
{
    return m_impl->baseAttributes();
}

std::vector<AbstractAttribute*> ObjectType::ownAttributes() const
{
    return m_impl->ownAttributes();
}

std::vector<AbstractAttribute*> ObjectType::attributes() const
{
    return m_impl->attributes();
}

std::vector<AbstractAttribute*> ObjectType::supportedAttributes() const
{
    return m_impl->supportedAttributes();
}

std::vector<AbstractAttribute*> ObjectType::supportedOwnAttributes() const
{
    return m_impl->supportedOwnAttributes();
}

bool ObjectType::hasEverBeenSupported() const
{
    return m_impl->hasEverBeenSupported();
}

bool ObjectType::isSupported(nx::Uuid engineId, nx::Uuid deviceId) const
{
    return m_impl->isSupported(engineId, deviceId);
}

bool ObjectType::isReachable() const
{
    return m_impl->isReachable();
}

bool ObjectType::isNonIndexable() const
{
    const ObjectTypeDescriptor& descriptor = m_impl->descriptor();
    return descriptor.flags.testFlag(ObjectTypeFlag::nonIndexable)
        || descriptor.flags.testFlag(ObjectTypeFlag::liveOnly);
}

bool ObjectType::isLiveOnly() const
{
    const ObjectTypeDescriptor& descriptor = m_impl->descriptor();
    return descriptor.flags.testFlag(ObjectTypeFlag::liveOnly);
}

const std::vector<AbstractScope*>& ObjectType::scopes() const
{
    return m_impl->scopes();
}

ObjectTypeDescriptor ObjectType::serialize() const
{
    return m_impl->serialize();
}

void ObjectType::addDerivedType(AbstractObjectType* derivedType)
{
    m_impl->addDerivedType(derivedType);
}

void ObjectType::resolveReachability()
{
    m_impl->resolveReachability();
}

void ObjectType::resolveReachability(bool hasPublicDescendants)
{
    m_impl->resolveReachability(hasPublicDescendants);
}

void ObjectType::resolveScopes(InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    m_impl->resolveScopes(inOutInternalState, errorHandler);
}

void ObjectType::resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    m_impl->resolve(inOutInternalState, errorHandler);
}

void ObjectType::resolveSupportedAttributes(
    InternalState* inOutInternalState, ErrorHandler* errorHandler)
{
    m_impl->resolveSupportedAttributes(inOutInternalState, errorHandler);
}

} // namespace nx::analytics::taxonomy
