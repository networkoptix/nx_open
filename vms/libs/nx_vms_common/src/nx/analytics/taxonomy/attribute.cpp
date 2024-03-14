// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute.h"

#include <nx/analytics/taxonomy/common.h>
#include <nx/analytics/taxonomy/proxy_object_type.h>
#include <nx/analytics/taxonomy/utils.h>

namespace nx::analytics::taxonomy {

using namespace nx::vms::api::analytics;

Attribute::Attribute(
    nx::vms::api::analytics::AttributeDescription attributeDescription,
    QObject* parent)
    :
    AbstractAttribute(parent),
    m_attributeDescription(std::move(attributeDescription))
{
    NX_ASSERT(m_attributeDescription.type == nx::vms::api::analytics::AttributeType::boolean
        || m_attributeDescription.type == nx::vms::api::analytics::AttributeType::string
        || m_attributeDescription.type == nx::vms::api::analytics::AttributeType::number);
}

Attribute::Attribute(
    nx::vms::api::analytics::AttributeDescription attributeDescription,
    AbstractObjectType* objectType,
    QObject* parent)
    :
    AbstractAttribute(parent),
    m_attributeDescription(std::move(attributeDescription)),
    m_objectType(objectType)
{
}

Attribute::Attribute(
    nx::vms::api::analytics::AttributeDescription attributeDescription,
    AbstractEnumType* enumType,
    QObject* parent)
    :
    AbstractAttribute(parent),
    m_attributeDescription(std::move(attributeDescription)),
    m_enumType(enumType)
{
    NX_ASSERT(m_attributeDescription.type == nx::vms::api::analytics::AttributeType::enumeration);
}

Attribute::Attribute(
    nx::vms::api::analytics::AttributeDescription attributeDescription,
    AbstractColorType* colorType,
    QObject* parent)
    :
    AbstractAttribute(parent),
    m_attributeDescription(std::move(attributeDescription)),
    m_colorType(colorType)
{
    NX_ASSERT(m_attributeDescription.type == nx::vms::api::analytics::AttributeType::color);
}

QString Attribute::name() const
{
    return m_attributeDescription.name;
}

AbstractAttribute::Type Attribute::type() const
{
    return fromDescriptorAttributeType(m_attributeDescription.type);
}

QString Attribute::subtype() const
{
    if (m_attributeDescription.type == AttributeType::number
        && (!m_attributeDescription.subtype || m_attributeDescription.subtype->isEmpty())
        && m_base)
    {
        return m_base->subtype();
    }

    return m_attributeDescription.subtype.value_or(QString());
}

AbstractEnumType* Attribute::enumType() const
{
    if (!NX_ASSERT(fromDescriptorAttributeType(m_attributeDescription.type) == Type::enumeration))
        return nullptr;

    const bool isSystemSubtype = m_attributeDescription.subtype == kIntegerAttributeSubtype
        || m_attributeDescription.subtype == kFloatAttributeSubtype;

    if (!NX_ASSERT(!isSystemSubtype))
        return nullptr;

    return m_enumType;
}

AbstractObjectType* Attribute::objectType() const
{
    if (!NX_ASSERT(fromDescriptorAttributeType(m_attributeDescription.type) == Type::object))
        return nullptr;

    const bool isSystemSubtype = m_attributeDescription.subtype == kIntegerAttributeSubtype
        || m_attributeDescription.subtype == kFloatAttributeSubtype;

    if (!NX_ASSERT(!isSystemSubtype))
        return nullptr;

    return m_objectType;
}

AbstractColorType* Attribute::colorType() const
{
    if (!NX_ASSERT(fromDescriptorAttributeType(m_attributeDescription.type) == Type::color))
        return nullptr;

    const bool isSystemSubtype = m_attributeDescription.subtype == kIntegerAttributeSubtype
        || m_attributeDescription.subtype == kFloatAttributeSubtype;

    if (!NX_ASSERT(!isSystemSubtype))
        return nullptr;

    return m_colorType;
}

QString Attribute::unit() const
{
    if (m_attributeDescription.unit && !m_attributeDescription.unit->isEmpty())
        return *m_attributeDescription.unit;

    if (m_base)
        return m_base->unit();

    return QString();
}

QVariant Attribute::minValue() const
{
    if (m_attributeDescription.minValue)
        return QVariant(*m_attributeDescription.minValue);

    if (m_base)
        return m_base->minValue();

    return QVariant();
}

QVariant Attribute::maxValue() const
{
    if (m_attributeDescription.maxValue)
        return QVariant(*m_attributeDescription.maxValue);

    if (m_base)
        return m_base->maxValue();

    return QVariant();
}

bool Attribute::isSupported(nx::Uuid engineId, nx::Uuid deviceId) const
{
    return false;
}

QString Attribute::condition() const
{
    return m_attributeDescription.condition.value_or(QString());
}

void Attribute::setBaseAttribute(AbstractAttribute* attribute)
{
    m_base = attribute;
}

} // namespace nx::analytics::taxonomy
