// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_object_type_attribute.h"

#include <nx/analytics/taxonomy/proxy_object_type.h>

namespace nx::analytics::taxonomy {

ProxyObjectTypeAttribute::ProxyObjectTypeAttribute(
    AbstractAttribute* proxiedAttribute,
    AttributeTree supportedAttributeTree)
    :
    AbstractAttribute(proxiedAttribute),
    m_proxiedAttribute(proxiedAttribute),
    m_supportedAttributeTree(std::move(supportedAttributeTree))
{
    NX_ASSERT(m_proxiedAttribute->type() == AbstractAttribute::Type::object,
        "Attribute %1: proxied attribuite type must be object");
}

QString ProxyObjectTypeAttribute::name() const
{
    return m_proxiedAttribute->name();
}

AbstractAttribute::Type ProxyObjectTypeAttribute::type() const
{
    return AbstractAttribute::Type::object;
}

QString ProxyObjectTypeAttribute::subtype() const
{
    return m_proxiedAttribute->subtype();
}

AbstractEnumType* ProxyObjectTypeAttribute::enumType() const
{
    return nullptr;
}

AbstractObjectType* ProxyObjectTypeAttribute::objectType() const
{
    if (m_proxyObjectType)
        return m_proxyObjectType;

    m_proxyObjectType = new ProxyObjectType(
        m_proxiedAttribute->objectType(), m_supportedAttributeTree);

    return m_proxyObjectType;
}

AbstractColorType* ProxyObjectTypeAttribute::colorType() const
{
    return nullptr;
}

QString ProxyObjectTypeAttribute::unit() const
{
    return QString();
}

QVariant ProxyObjectTypeAttribute::minValue() const
{
    return QVariant();
}

QVariant ProxyObjectTypeAttribute::maxValue() const
{
    return QVariant();
}

} // namespace nx::analytics::taxonomy
