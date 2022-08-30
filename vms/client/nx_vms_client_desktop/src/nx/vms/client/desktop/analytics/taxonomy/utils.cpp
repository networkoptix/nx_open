// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include "attribute.h"
#include "attribute_set.h"
#include "color_set.h"
#include "enumeration.h"

#include <nx/analytics/taxonomy/common.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

AbstractAttribute* mergeNumericAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    auto attribute = new Attribute(parent);
    attribute->setType(AbstractAttribute::Type::number);
    attribute->setName(taxonomyAttributes[0]->name());

    struct AttributeData
    {
        QString subtype;
        QString unit;
        QVariant minValue;
        QVariant maxValue;
    };

    AttributeData attributeData;
    attributeData.subtype = taxonomyAttributes[0]->subtype();
    attributeData.unit = taxonomyAttributes[0]->unit();
    attributeData.minValue = taxonomyAttributes[0]->minValue();
    attributeData.maxValue = taxonomyAttributes[0]->maxValue();

    for (const nx::analytics::taxonomy::AbstractAttribute* taxonomyAttribute: taxonomyAttributes)
    {
        if (!NX_ASSERT(taxonomyAttribute->type() ==
            nx::analytics::taxonomy::AbstractAttribute::Type::number))
        {
            continue;
        }

        if (taxonomyAttribute->subtype() != attributeData.subtype)
            attributeData.subtype = nx::analytics::taxonomy::kFloatAttributeSubtype;

        if (taxonomyAttribute->unit() != attributeData.unit)
            attributeData.unit = QString();

        if (!attributeData.minValue.isNull())
        {
            const QVariant minValue = taxonomyAttribute->minValue();
            if (minValue.isNull() || minValue.toFloat() < attributeData.minValue.toFloat())
                attributeData.minValue = minValue;
        }

        if (!attributeData.maxValue.isNull())
        {
            const QVariant maxValue = taxonomyAttribute->maxValue();
            if (maxValue.isNull() || maxValue.toFloat() > attributeData.maxValue.toFloat())
                attributeData.maxValue = maxValue;
        }
    }

    attribute->setSubtype(attributeData.subtype);
    attribute->setMinValue(attributeData.minValue);
    attribute->setMaxValue(attributeData.maxValue);
    attribute->setUnit(attributeData.unit);

    return attribute;
}

AbstractAttribute* mergeColorTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    auto attribute = new Attribute(parent);
    attribute->setType(AbstractAttribute::Type::colorSet);
    attribute->setName(taxonomyAttributes[0]->name());

    auto colorSet = new ColorSet(parent);
    for (const nx::analytics::taxonomy::AbstractAttribute* taxonomyAttribute: taxonomyAttributes)
    {
        if (!NX_ASSERT(taxonomyAttribute->type() ==
            nx::analytics::taxonomy::AbstractAttribute::Type::color))
        {
            continue;
        }

        colorSet->addColorType(taxonomyAttribute->colorType());
    }

    attribute->setColorSet(colorSet);
    return attribute;
}

AbstractAttribute* mergeEnumTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    auto attribute = new Attribute(parent);
    attribute->setType(AbstractAttribute::Type::enumeration);
    attribute->setName(taxonomyAttributes[0]->name());

    auto enumeration = new Enumeration(parent);
    for (const nx::analytics::taxonomy::AbstractAttribute* taxonomyAttribute: taxonomyAttributes)
    {
        if (!NX_ASSERT(taxonomyAttribute->type() ==
            nx::analytics::taxonomy::AbstractAttribute::Type::enumeration))
        {
            continue;
        }

        enumeration->addEnumType(taxonomyAttribute->enumType());
    }

    attribute->setEnumeration(enumeration);
    return attribute;
}

AbstractAttribute* mergeObjectTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    const AbstractStateViewFilter* filter,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    auto attribute = new Attribute(parent);
    attribute->setName(taxonomyAttributes[0]->name());
    attribute->setType(AbstractAttribute::Type::attributeSet);

    auto attributeSet = new AttributeSet(filter, parent);
    for (const nx::analytics::taxonomy::AbstractAttribute* taxonomyAttribute: taxonomyAttributes)
    {
        if (!NX_ASSERT(taxonomyAttribute->type() ==
            nx::analytics::taxonomy::AbstractAttribute::Type::object))
        {
            continue;
        }

        attributeSet->addObjectType(taxonomyAttribute->objectType());
    }

    attribute->setAttributeSet(attributeSet);
    return attribute;
}

nx::analytics::taxonomy::AbstractAttribute::Type attributeType(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nx::analytics::taxonomy::AbstractAttribute::Type::undefined;

    return taxonomyAttributes[0]->type();
}

AbstractAttribute::Type fromTaxonomyAttributeType(
    nx::analytics::taxonomy::AbstractAttribute::Type taxonomyAttributeType)
{
    switch (taxonomyAttributeType)
    {
        case nx::analytics::taxonomy::AbstractAttribute::Type::number:
            return AbstractAttribute::Type::number;
        case nx::analytics::taxonomy::AbstractAttribute::Type::enumeration:
            return AbstractAttribute::Type::enumeration;
        case nx::analytics::taxonomy::AbstractAttribute::Type::color:
            return AbstractAttribute::Type::colorSet;
        case nx::analytics::taxonomy::AbstractAttribute::Type::object:
            return AbstractAttribute::Type::attributeSet;
        case nx::analytics::taxonomy::AbstractAttribute::Type::boolean:
            return AbstractAttribute::Type::boolean;
        case nx::analytics::taxonomy::AbstractAttribute::Type::string:
            return AbstractAttribute::Type::string;
        default:
            return AbstractAttribute::Type::undefined;
    }
}

AbstractAttribute* wrapAttribute(
    const nx::analytics::taxonomy::AbstractAttribute* taxonomyAttribute,
    QObject* parent)
{
    auto attribute = new Attribute(parent);
    attribute->setName(taxonomyAttribute->name());
    attribute->setType(fromTaxonomyAttributeType(taxonomyAttribute->type()));
    return attribute;
}

AbstractAttribute* mergeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    const AbstractStateViewFilter* filter,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    switch (attributeType(taxonomyAttributes))
    {
        case nx::analytics::taxonomy::AbstractAttribute::Type::number:
            return mergeNumericAttributes(taxonomyAttributes, parent);
        case nx::analytics::taxonomy::AbstractAttribute::Type::enumeration:
            return mergeEnumTypeAttributes(taxonomyAttributes, parent);
        case nx::analytics::taxonomy::AbstractAttribute::Type::color:
            return mergeColorTypeAttributes(taxonomyAttributes, parent);
        case nx::analytics::taxonomy::AbstractAttribute::Type::object:
            return mergeObjectTypeAttributes(taxonomyAttributes, filter, parent);
        case nx::analytics::taxonomy::AbstractAttribute::Type::boolean:
        case nx::analytics::taxonomy::AbstractAttribute::Type::string:
            return wrapAttribute(taxonomyAttributes[0], parent);
        default:
            return nullptr;
    }
}

std::vector<AbstractAttribute*> resolveAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractObjectType*>& objectTypes,
    const AbstractStateViewFilter* filter,
    QObject* parent)
{
    std::map<
        QString,
        std::vector<const nx::analytics::taxonomy::AbstractAttribute*>> attributesToMerge;
    std::vector<QString> orderedAttributeNames;
    std::vector<AbstractAttribute*> result;

    for (const auto& objectType: objectTypes)
    {
        for (const nx::analytics::taxonomy::AbstractAttribute* attribute:
            objectType->supportedAttributes())
        {
            if (filter && !filter->matches(attribute))
                continue;

            const QString attributeName = attribute->name();
            if (attributesToMerge.find(attributeName) == attributesToMerge.cend())
                orderedAttributeNames.push_back(attributeName);

            attributesToMerge[attributeName].push_back(attribute);
        }
    }

    for (const QString& attributeName: orderedAttributeNames)
        result.push_back(mergeAttributes(attributesToMerge[attributeName], filter, parent));

    return result;
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
