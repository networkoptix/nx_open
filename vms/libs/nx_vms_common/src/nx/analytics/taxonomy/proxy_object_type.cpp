// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_object_type.h"

#include <nx/analytics/taxonomy/utils.h>

namespace nx::analytics::taxonomy {

ProxyObjectType::ProxyObjectType(
    AbstractObjectType* proxiedObjectType,
    AttributeTree supportedAttributeTree)
    :
    AbstractObjectType(proxiedObjectType),
    m_proxiedObjectType(proxiedObjectType),
    m_supportedAttributeTree(std::move(supportedAttributeTree))
{
}

QString ProxyObjectType::id() const
{
    return m_proxiedObjectType->id();
}

QString ProxyObjectType::name() const
{
    return m_proxiedObjectType->name();
}

QString ProxyObjectType::icon() const
{
    return m_proxiedObjectType->icon();
}

AbstractObjectType* ProxyObjectType::base() const
{
    return m_proxiedObjectType->base();
}

std::vector<AbstractObjectType*> ProxyObjectType::derivedTypes() const
{
    return m_proxiedObjectType->derivedTypes();
}

std::vector<AbstractAttribute*> ProxyObjectType::baseAttributes() const
{
    return m_proxiedObjectType->baseAttributes();
}

std::vector<AbstractAttribute*> ProxyObjectType::ownAttributes() const
{
    return m_proxiedObjectType->ownAttributes();
}

std::vector<AbstractAttribute*> ProxyObjectType::supportedAttributes() const
{
    if (m_supportedAttributes)
        return *m_supportedAttributes;

    m_supportedAttributes = makeSupportedAttributes(this->attributes(), m_supportedAttributeTree);

    return *m_supportedAttributes;
}

std::vector<AbstractAttribute*> ProxyObjectType::supportedOwnAttributes() const
{
    if (m_supportedOwnAttributes)
        return *m_supportedOwnAttributes;

    m_supportedAttributes = makeSupportedAttributes(
        this->ownAttributes(), m_supportedAttributeTree);

    return *m_supportedOwnAttributes;
}

std::vector<AbstractAttribute*> ProxyObjectType::attributes() const
{
    return m_proxiedObjectType->attributes();
}

bool ProxyObjectType::hasEverBeenSupported() const
{
    return m_proxiedObjectType->hasEverBeenSupported();
}

bool ProxyObjectType::isPrivate() const
{
    return m_proxiedObjectType->isPrivate();
}

bool ProxyObjectType::isLiveOnly() const
{
    return m_proxiedObjectType->isLiveOnly();
}

bool ProxyObjectType::isNonIndexable() const
{
    return m_proxiedObjectType->isNonIndexable();
}

std::vector<AbstractScope*> ProxyObjectType::scopes() const
{
    return m_proxiedObjectType->scopes();
}

nx::vms::api::analytics::ObjectTypeDescriptor ProxyObjectType::serialize() const
{
    return m_proxiedObjectType->serialize();
}

} // namespace nx::analytics::taxonomy
