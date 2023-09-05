// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute_resolver.h"

#include <nx/analytics/taxonomy/common.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/analytics/taxonomy/enum_type.h>
#include <nx/analytics/taxonomy/color_type.h>
#include <nx/analytics/taxonomy/error_handler.h>

namespace nx::analytics::taxonomy {

using namespace nx::vms::api::analytics;

AttributeResolver::AttributeResolver(Context context, ErrorHandler* errorHandler):
    m_context(std::move(context)),
    m_errorHandler(errorHandler)
{
    for (auto attribute : m_context.baseAttributes)
        m_baseAttributeByName[attribute->name()] = attribute;
}

void AttributeResolver::resolve()
{
    resolveOmittedBaseAttributes();
    resolveOwnAttributes();
}

void AttributeResolver::resolveOmittedBaseAttributes()
{
    if (m_context.baseTypeId.isEmpty() && !m_context.omittedBaseAttributeNames->empty())
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: omitted attribute list is not empty but there is no base type",
                m_context.typeName, m_context.typeId)});

        *m_context.omittedBaseAttributeNames = std::vector<QString>();
    }

    std::set<QString> uniqueOmittedBaseAttributeNames;
    std::vector<QString> omittedBaseAttributeNames;
    for (const QString& omittedBaseAttributeName : *m_context.omittedBaseAttributeNames)
    {
        if (contains(uniqueOmittedBaseAttributeNames, omittedBaseAttributeName))
        {
            m_errorHandler->handleError(
                ProcessingError{NX_FMT("%1 %2: duplicate in the omitted base attribute list (%3)",
                    m_context.typeName, m_context.typeId, omittedBaseAttributeName)});

            continue;
        }

        if (!contains(m_baseAttributeByName, omittedBaseAttributeName))
        {
            m_errorHandler->handleError(
                ProcessingError{NX_FMT(
                    "%1 %2: base attribute %3 is declared as omitted but base %4 (%5) "
                    "doesn't contain such an attribute",
                    m_context.typeName, m_context.typeId, omittedBaseAttributeName,
                    m_context.typeName, m_context.baseTypeId)});

            continue;
        }

        uniqueOmittedBaseAttributeNames.insert(omittedBaseAttributeName);
        omittedBaseAttributeNames.push_back(omittedBaseAttributeName);
    }

    *m_context.omittedBaseAttributeNames = omittedBaseAttributeNames;
}

void AttributeResolver::fillAttributeCandidateList(
    const std::vector<AttributeDescription>& attributes,
    const std::map<QString, std::vector<AttributeDescription>>& attributeLists,
    std::vector<AttributeDescription>* outAttributeCandidateList,
    const std::set<QString>& uniqueAttributeListIds)
{
    for (const AttributeDescription& attribute: attributes)
    {
        if (attribute.attributeList.isEmpty())
        {
            outAttributeCandidateList->push_back(attribute);
        }
        else
        {
            if (!attributeLists.contains(attribute.attributeList))
            {
                m_errorHandler->handleError(
                    ProcessingError{NX_FMT(
                        "%1 %2: Attribute List %3 doesn't exist.",
                        m_context.typeName, m_context.typeId, attribute.attributeList)});
                continue;
            }

            if (uniqueAttributeListIds.contains(attribute.attributeList))
            {
                m_errorHandler->handleError(
                    ProcessingError{NX_FMT(
                        "%1 %2: Cyclic Attribute Lists detected: %3.",
                        m_context.typeName, m_context.typeId,
                        nx::containerString(uniqueAttributeListIds))});
                continue;
            }

            const std::vector<AttributeDescription> attributeList =
                attributeLists.at(attribute.attributeList);

            std::set<QString> currentUniqueAttributeListIds = uniqueAttributeListIds;
            currentUniqueAttributeListIds.insert(attribute.attributeList);
            fillAttributeCandidateList(
                attributeList,
                attributeLists,
                outAttributeCandidateList,
                currentUniqueAttributeListIds);
        }
    }
}

void AttributeResolver::resolveOwnAttributes()
{
    std::set<QString> uniqueAttributeNames;
    std::set<QString> conditionalAttributeNames;
    std::vector<AttributeDescription> ownAttributes;

    std::vector<AttributeDescription> attributeCandidates;
    fillAttributeCandidateList(
        *m_context.ownAttributes,
        m_context.internalState->attributeListById,
        &attributeCandidates,
        std::set<QString>());

    for (AttributeDescription& attribute: attributeCandidates)
    {
        if (attribute.name.isEmpty())
        {
            m_errorHandler->handleError(
                ProcessingError{NX_FMT(
                    "%1 %2: empty attribute name",
                    m_context.typeName, m_context.typeId)});

            continue;
        }

        if (!attribute.condition.isEmpty())
            conditionalAttributeNames.insert(attribute.name);

        if (uniqueAttributeNames.contains(attribute.name)
            && (!conditionalAttributeNames.contains(attribute.name)
                || attribute.condition.isEmpty()))
        {
            m_errorHandler->handleError(
                ProcessingError{NX_FMT(
                    "%1 %2: duplicate in the attribute list (%3)",
                    m_context.typeName, m_context.typeId, attribute.name)});

            continue;
        }

        auto baseAttributeItr = m_baseAttributeByName.find(attribute.name);
        AbstractAttribute* baseAttribute =
            (baseAttributeItr != m_baseAttributeByName.cend()) ? baseAttributeItr->second : nullptr;

        if (baseAttribute && !baseAttribute->condition().isEmpty())
        {
            m_errorHandler->handleError(
                ProcessingError{NX_FMT(
                    "%1 %2: using conditional Attribute as base (%3)",
                    m_context.typeName, m_context.typeId, attribute.name)});

            continue;
        }

        if (Attribute* resolvedAttribute = resolveOwnAttribute(&attribute, baseAttribute);
            resolvedAttribute)
        {
            uniqueAttributeNames.insert(attribute.name);
            ownAttributes.push_back(attribute);

            resolvedAttribute->setBaseAttribute(baseAttribute);
            resolvedAttribute->setParent(m_context.owner);
            m_context.resolvedOwnAttributes->push_back(resolvedAttribute);
        }
    }

    *m_context.ownAttributes = ownAttributes;
}

Attribute* AttributeResolver::resolveOwnAttribute(
    AttributeDescription* inOutAttributeDescription, const AbstractAttribute* baseAttribute)
{
    if (baseAttribute)
    {
        const AttributeType baseAttributeType = toDescriptorAttributeType(baseAttribute->type());
        if (baseAttributeType != inOutAttributeDescription->type)
        {
            m_errorHandler->handleError(
                ProcessingError{NX_FMT(
                    "%1 %2: base type (%3) has an attribute with the same name (%4) but "
                    "different type. Base attribute type: %5, attribute type: %6",
                    m_context.typeName, m_context.typeId, m_context.baseTypeId,
                    baseAttribute->name(), baseAttributeType, inOutAttributeDescription->type)});

            return nullptr;
        }
    }

    // The type can be omitted for enum and color attributes, so we either try to deduce the type
    // from the subtype, or fail.
    if (inOutAttributeDescription->type == std::nullopt)
        return tryToDeduceTypeFromSubtype(inOutAttributeDescription, baseAttribute);

    AttributeType type = inOutAttributeDescription->type.value();

    switch (type)
    {
        case AttributeType::boolean:
            return resolveBooleanAttribute(inOutAttributeDescription, baseAttribute);
        case AttributeType::number:
            return resolveNumberAttribute(inOutAttributeDescription, baseAttribute);
        case AttributeType::string:
            return resolveStringAttribute(inOutAttributeDescription, baseAttribute);
        case AttributeType::enumeration:
            return resolveEnumerationAttribute(inOutAttributeDescription, baseAttribute);
        case AttributeType::color:
            return resolveColorAttribute(inOutAttributeDescription, baseAttribute);
        case AttributeType::object:
            return resolveObjectAttribute(inOutAttributeDescription, baseAttribute);
        case AttributeType::undefined:
        {
            m_errorHandler->handleError(
                ProcessingError{
                    NX_FMT("%1 %2: undefined attribute type",
                        m_context.typeName, m_context.typeId)});
            return nullptr;
        }
        default:
        {
            const QString errorDetails = NX_FMT("%1 %2: unknown attribute type (%3)",
                m_context.typeName, m_context.typeId, (int) type);

            m_errorHandler->handleError(ProcessingError{errorDetails});
            NX_ASSERT(false, errorDetails);

            return nullptr;
        }
    }
}

Attribute* AttributeResolver::tryToDeduceTypeFromSubtype(
    nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
    const AbstractAttribute* baseAttribute)
{
    const QString& subtypeId = inOutAttributeDescription->subtype;
    
    const EnumType* enumType = m_context.internalState->getTypeById<EnumType>(subtypeId);
    const ColorType* colorType = m_context.internalState->getTypeById<ColorType>(subtypeId);
    
    if (enumType && colorType)
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: The \"type\" is omitted, but the subtype %3 is ambiguous (Enum or Color), "
                "declared in the attribute %4",
                m_context.typeName, m_context.typeId, subtypeId, inOutAttributeDescription->name)});

        return nullptr;
    }
    else if (!enumType && !colorType)
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: The \"type\" is omitted, but the subtype %3 doesn't exist, "
                "declared in the attribute %4",
                m_context.typeName, m_context.typeId, subtypeId, inOutAttributeDescription->name)});

        return nullptr;
    }
    else if (enumType)
    {
        inOutAttributeDescription->type = AttributeType::enumeration;
        return resolveEnumerationAttribute(inOutAttributeDescription, baseAttribute);
    }
    else if (colorType)
    {
        inOutAttributeDescription->type = AttributeType::color;
        return resolveColorAttribute(inOutAttributeDescription, baseAttribute);
    }

    return nullptr;
}

Attribute* AttributeResolver::resolveBooleanAttribute(
    nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
    const AbstractAttribute* /*baseAttribute*/)
{
    return new Attribute(*inOutAttributeDescription);
}

Attribute* AttributeResolver::resolveStringAttribute(
    nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
    const AbstractAttribute* /*baseAttribute*/)
{
    return new Attribute(*inOutAttributeDescription);
}

Attribute* AttributeResolver::resolveNumberAttribute(
    AttributeDescription* inOutAttributeDescription, const AbstractAttribute* baseAttribute)
{
    if (!resolveNumericSubtype(inOutAttributeDescription, baseAttribute))
        return nullptr;

    if (!resolveNumericRange(inOutAttributeDescription, baseAttribute))
        return nullptr;

    if (!resolveUnit(inOutAttributeDescription, baseAttribute))
        return nullptr;

    return new Attribute(*inOutAttributeDescription);
}

bool AttributeResolver::resolveNumericSubtype(
    nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
    const AbstractAttribute* baseAttribute)
{
    QString& subtype = inOutAttributeDescription->subtype;
    const QString baseAttributeSubtype = baseAttribute ? baseAttribute->subtype() : QString();

    if (!subtype.isEmpty())
    {
        if (subtype != kIntegerAttributeSubtype && subtype != kFloatAttributeSubtype)
        {
            m_errorHandler->handleError(
                ProcessingError{NX_FMT(
                    "%1 %2: attribute %3 has invalid numeric attribute subtype (%4)",
                        m_context.typeName, m_context.typeId,
                        inOutAttributeDescription->name, subtype)});

            subtype = QString();
        }
        else if (baseAttribute
            && !baseAttributeSubtype.isEmpty()
            && baseAttributeSubtype != subtype)
        {
            m_errorHandler->handleError(
                ProcessingError{NX_FMT(
                    "%1 %2: subtype (%3) of an attribute %4 differs from its base type (%5) "
                    "attribute subtype (%6)",
                    m_context.typeName, m_context.typeId, subtype,
                    inOutAttributeDescription->name, m_context.baseTypeId, baseAttributeSubtype)});

            subtype = QString();
        }
    }

    return true;
}

bool AttributeResolver::resolveNumericRange(
    nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
    const AbstractAttribute* baseAttribute)
{
    std::optional<double>& minValue = inOutAttributeDescription->minValue;
    std::optional<double>& maxValue = inOutAttributeDescription->maxValue;

    std::optional<double> baseAttributeMinValue =
        (baseAttribute && !baseAttribute->minValue().isNull())
            ? std::optional<double>(baseAttribute->minValue().toDouble())
            : std::nullopt;

    std::optional<double> baseAttributeMaxValue =
        (baseAttribute && !baseAttribute->maxValue().isNull())
            ? std::optional<double>(baseAttribute->maxValue().toDouble())
            : std::nullopt;


    if (minValue && maxValue && *minValue > * maxValue)
    {
        m_errorHandler->handleError(
            ProcessingError{ NX_FMT(
                "%1 %2: attribute %3 incorrect range [%4, %5]",
                m_context.typeName, m_context.typeId, inOutAttributeDescription->name,
                *minValue, *maxValue, m_context.baseTypeId) });

        minValue.reset();
        maxValue.reset();

        return true;
    }

    if (minValue && baseAttributeMinValue && *minValue < *baseAttributeMinValue)
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: attribute %3 min value (%4) is less than min value (%5) "
                "of the same attribute of the base type (%6)",
                m_context.typeName, m_context.typeId, inOutAttributeDescription->name,
                *minValue, *baseAttributeMinValue, m_context.baseTypeId)});

        minValue.reset();
    }

    if (maxValue && baseAttributeMaxValue && *maxValue > *baseAttributeMaxValue)
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: attribute %3 max value (%4) is more than max value (%5) "
                "of the same attribute of the base type (%6)",
                m_context.typeName, m_context.typeId, inOutAttributeDescription->name,
                *maxValue, *baseAttributeMaxValue, m_context.baseTypeId)});

        maxValue.reset();
    }

    return true;
}

bool AttributeResolver::resolveUnit(
    nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
    const AbstractAttribute* baseAttribute)
{
    if (!baseAttribute || baseAttribute->unit().isEmpty())
        return true;

    QString& unit = inOutAttributeDescription->unit;
    const QString baseAttributeUnit = baseAttribute->unit();

    if (!unit.isEmpty() && unit != baseAttributeUnit)
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: unit (%3) is not equal to the unit (%4) of the base type (%5)",
                m_context.typeName, m_context.typeId, unit,
                baseAttributeUnit, m_context.baseTypeId)});

        unit = QString();
    }

    return true;
}

Attribute* AttributeResolver::resolveEnumerationAttribute(
    AttributeDescription* inOutAttributeDescription, const AbstractAttribute* baseAttribute)
{
    const QString enumId = inOutAttributeDescription->subtype;
    EnumType* enumType = m_context.internalState->getTypeById<EnumType>(enumId);
    if (!enumType)
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: unable to find an Enum Type %3 declared in the attribute %4 subtype",
                m_context.typeName, m_context.typeId, enumId, inOutAttributeDescription->name)});

        return nullptr;
    }

    return new Attribute(*inOutAttributeDescription, enumType);
}

Attribute* AttributeResolver::resolveColorAttribute(
    AttributeDescription* inOutAttributeDescription, const AbstractAttribute* baseAttribute)
{
    const QString colorTypeId = inOutAttributeDescription->subtype;
    ColorType* colorType = m_context.internalState->getTypeById<ColorType>(colorTypeId);
    if (!colorType)
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: unable to find a Color Type %3 declared in the attribute %4 subtype",
                m_context.typeName, m_context.typeId,
                colorTypeId, inOutAttributeDescription->name)});

        return nullptr;
    }

    return new Attribute(*inOutAttributeDescription, colorType);
}

Attribute* AttributeResolver::resolveObjectAttribute(
    AttributeDescription* inOutAttributeDescription, const AbstractAttribute* baseAttribute)
{
    const QString objectTypeId = inOutAttributeDescription->subtype;
    ObjectType* objectType = m_context.internalState->getTypeById<ObjectType>(objectTypeId);
    if (!objectType)
    {
        m_errorHandler->handleError(
            ProcessingError{NX_FMT(
                "%1 %2: unable to find an Object Type %3 declared in the attribute %4 subtype",
                m_context.typeName, m_context.typeId,
                objectTypeId, inOutAttributeDescription->name)});

        return nullptr;
    }

    return new Attribute(*inOutAttributeDescription, objectType);
}

} // namespace nx::analytics::taxonomy
