// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_attribute.h"

#include <nx/analytics/taxonomy/proxy_object_type.h>

namespace nx::analytics::taxonomy {

ProxyAttribute::ProxyAttribute(
    AbstractAttribute* attribute,
    AttributeSupportInfoTree attributeSupportInfoTree,
    AbstractResourceSupportProxy* resourceSupportProxy,
    QString prefix,
    QString rootParentTypeId,
    EntityType rootEntityType)
    :
    AbstractAttribute(attribute),
    m_proxiedAttribute(attribute),
    m_supportByEngine(std::move(attributeSupportInfoTree.supportByEngine)),
    m_prefix(std::move(prefix)),
    m_rootParentTypeId(std::move(rootParentTypeId)),
    m_rootEntityType(rootEntityType),
    m_resourceSupportProxy(resourceSupportProxy)
{
    if (!NX_ASSERT(m_proxiedAttribute))
        return;

    if (m_proxiedAttribute->type() == Type::object)
    {
        m_proxyObjectType = new ProxyObjectType(
            m_proxiedAttribute->objectType(),
            std::move(attributeSupportInfoTree.nestedAttributeSupportInfo),
            m_resourceSupportProxy,
            m_prefix + m_proxiedAttribute->name() + ".",
            m_rootParentTypeId,
            m_rootEntityType);
    }
}

ProxyAttribute::~ProxyAttribute()
{
}

QString ProxyAttribute::name() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->name();

    return QString();
}

AbstractAttribute::Type ProxyAttribute::type() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->type();

    return Type::undefined;
}

QString ProxyAttribute::subtype() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->subtype();

    return QString();
}

AbstractEnumType* ProxyAttribute::enumType() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->enumType();

    return nullptr;
}

AbstractObjectType* ProxyAttribute::objectType() const
{
    if (m_proxyObjectType)
       return m_proxyObjectType;

    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->objectType();

    return nullptr;
}

AbstractColorType* ProxyAttribute::colorType() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->colorType();

    return nullptr;
}

QString ProxyAttribute::unit() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->unit();

    return QString();
}

QVariant ProxyAttribute::minValue() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->minValue();

    return QVariant();
}

QVariant ProxyAttribute::maxValue() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->maxValue();

    return QVariant();
}

bool ProxyAttribute::isSupported(nx::Uuid engineId, nx::Uuid deviceId) const
{
    if (m_supportByEngine.empty())
        return false;

    if (engineId.isNull())
        return true;

    if (!m_supportByEngine.contains(engineId))
        return false;

    return m_resourceSupportProxy->isEntityTypeAttributeSupported(
        m_rootEntityType, m_rootParentTypeId, m_prefix + m_proxiedAttribute->name(), deviceId, engineId);
}

QString ProxyAttribute::condition() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->condition();

    return QString();
}

} // namespace nx::analytics::taxonomy
