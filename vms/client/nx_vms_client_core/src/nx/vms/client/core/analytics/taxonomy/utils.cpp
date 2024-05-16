// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <analytics/db/text_search_utils.h>
#include <nx/analytics/taxonomy/common.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>

#include "abstract_state_view_filter.h"
#include "attribute.h"
#include "attribute_set.h"
#include "color_set.h"
#include "enumeration.h"

namespace nx::vms::client::core::analytics::taxonomy {

namespace {

Attribute::Type fromTaxonomyAttributeType(
    nx::analytics::taxonomy::AbstractAttribute::Type taxonomyAttributeType)
{
    switch (taxonomyAttributeType)
    {
        case nx::analytics::taxonomy::AbstractAttribute::Type::number:
            return Attribute::Type::number;
        case nx::analytics::taxonomy::AbstractAttribute::Type::enumeration:
            return Attribute::Type::enumeration;
        case nx::analytics::taxonomy::AbstractAttribute::Type::color:
            return Attribute::Type::colorSet;
        case nx::analytics::taxonomy::AbstractAttribute::Type::object:
            return Attribute::Type::attributeSet;
        case nx::analytics::taxonomy::AbstractAttribute::Type::boolean:
            return Attribute::Type::boolean;
        case nx::analytics::taxonomy::AbstractAttribute::Type::string:
            return Attribute::Type::string;
        default:
            return Attribute::Type::undefined;
    }
}

} // namespace

Attribute* mergeNumericAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    auto attribute = new Attribute(parent);
    attribute->type = Attribute::Type::number;
    attribute->name = taxonomyAttributes[0]->name();

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

    attribute->subtype = attributeData.subtype;
    attribute->minValue = attributeData.minValue;
    attribute->maxValue = attributeData.maxValue;
    attribute->unit = attributeData.unit;

    return attribute;
}

Attribute* mergeColorTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    auto attribute = new Attribute(parent);
    attribute->type = Attribute::Type::colorSet;
    attribute->name = taxonomyAttributes[0]->name();

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

    attribute->colorSet = colorSet;
    return attribute;
}

Attribute* mergeEnumTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    auto attribute = new Attribute(parent);
    attribute->type = Attribute::Type::enumeration;
    attribute->name = taxonomyAttributes[0]->name();

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

    attribute->enumeration = enumeration;
    return attribute;
}

Attribute* mergeObjectTypeAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes,
    const AbstractStateViewFilter* filter,
    QObject* parent)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nullptr;

    auto attribute = new Attribute(parent);
    attribute->name = taxonomyAttributes[0]->name();
    attribute->type = Attribute::Type::attributeSet;

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

    attribute->attributeSet = attributeSet;
    return attribute;
}

nx::analytics::taxonomy::AbstractAttribute::Type attributeType(
    const std::vector<const nx::analytics::taxonomy::AbstractAttribute*>& taxonomyAttributes)
{
    if (!NX_ASSERT(!taxonomyAttributes.empty()) || !NX_ASSERT(taxonomyAttributes[0]))
        return nx::analytics::taxonomy::AbstractAttribute::Type::undefined;

    return taxonomyAttributes[0]->type();
}

Attribute* wrapAttribute(
    const nx::analytics::taxonomy::AbstractAttribute* taxonomyAttribute,
    QObject* parent)
{
    auto attribute = new Attribute(parent);
    attribute->name = taxonomyAttribute->name();
    attribute->type = fromTaxonomyAttributeType(taxonomyAttribute->type());
    return attribute;
}

Attribute* mergeAttributes(
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

void determineConditionReferences(
    const std::vector<Attribute*>& attributes,
    const QSet<QString>& attributeConditions)
{
    using namespace nx::analytics::db;

    QSet<QString> references;
    UserTextSearchExpressionParser parser;

    for (const QString& conditionText: attributeConditions)
    {
        parser.parse(
            conditionText,
            [&references](const TextSearchCondition& condition)
            {
                references << condition.name.toLower();
            });
    }

    for (Attribute* attribute: attributes)
        attribute->isReferencedInCondition = references.contains(attribute->name.toLower());
}

std::vector<Attribute*> resolveAttributes(
    const std::vector<const nx::analytics::taxonomy::AbstractObjectType*>& objectTypes,
    const AbstractStateViewFilter* filter,
    QObject* parent)
{
    std::map<
        QString,
        std::vector<const nx::analytics::taxonomy::AbstractAttribute*>> attributesToMerge;
    std::vector<QString> orderedAttributeNames;
    std::vector<Attribute*> result;
    QSet<QString> conditions;

    for (const auto& objectType: objectTypes)
    {
        for (const nx::analytics::taxonomy::AbstractAttribute* attribute:
            objectType->supportedAttributes())
        {
            if (!attribute->condition().isEmpty())
                conditions.insert(attribute->condition()); //< Save condition before filtering.

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

    determineConditionReferences(result, conditions);

    return result;
}

QString makeEnumValuesExact(
    const QString& filter,
    const AttributeHelper* attributeHelper,
    const QVector<QString>& objectTypeIds)
{
    using namespace nx::analytics::db;
    using namespace nx::analytics::taxonomy;

    std::vector<TextSearchCondition> conditions = UserTextSearchExpressionParser().parse(filter);
    for (TextSearchCondition& condition: conditions)
    {
        const AbstractAttribute* attribute =
            attributeHelper->findAttribute(condition.name, objectTypeIds);

        const bool isFullMatch = attribute
            && (attribute->type() == AbstractAttribute::Type::enumeration
                || attribute->type() == AbstractAttribute::Type::color);

        if (isFullMatch)
        {
            condition.valueToken.matchesFromStart = isFullMatch;
            condition.valueToken.matchesTillEnd = isFullMatch;
        }
    };

    return serializeTextSearchConditions(conditions);
}

} // namespace nx::vms::client::core::analytics::taxonomy
