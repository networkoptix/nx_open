// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_attribute.h"

#include <nx/analytics/taxonomy/proxy_object_type.h>

namespace nx::analytics::taxonomy {

ProxyAttribute::ProxyAttribute(
    AbstractAttribute* attribute,
    AttributeSupportInfoTree attributeSupportInfoTree,
    AbstractResourceSupportProxy* resourceSupportProxy,
    std::string prefix,
    std::string rootParentTypeId,
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
        m_proxyObjectType = std::make_unique<ProxyObjectType>(
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

const std::string& ProxyAttribute::name() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->name();

    static const std::string kEmptyString;
    return kEmptyString;
}

AbstractAttribute::Type ProxyAttribute::type() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->type();

    return Type::undefined;
}

const std::string& ProxyAttribute::subtype() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->subtype();

    static const std::string kEmptyString;
    return kEmptyString;
}

AbstractEnumType* ProxyAttribute::enumType() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->enumType();

    return nullptr;
}

ObjectType* ProxyAttribute::objectType() const
{
    if (m_proxyObjectType)
       return m_proxyObjectType.get();

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

const std::string& ProxyAttribute::unit() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->unit();

    static const std::string kEmptyString;
    return kEmptyString;
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

const std::string& ProxyAttribute::condition() const
{
    if (NX_ASSERT(m_proxiedAttribute))
        return m_proxiedAttribute->condition();

    static const std::string kEmptyString;
    return kEmptyString;
}

} // namespace nx::analytics::taxonomy
