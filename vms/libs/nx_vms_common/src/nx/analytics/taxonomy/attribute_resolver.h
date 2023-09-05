// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/analytics/taxonomy/attribute.h>
#include <nx/analytics/taxonomy/internal_state.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class ErrorHandler;

class AttributeResolver
{
public:
    struct Context
    {
        QString typeName;
        QString typeId;
        QString baseTypeId;

        QObject* owner = nullptr;

        std::vector<nx::vms::api::analytics::AttributeDescription>* ownAttributes = nullptr;
        std::vector<QString>* omittedBaseAttributeNames = nullptr;
        std::vector<AbstractAttribute*>* resolvedOwnAttributes = nullptr;

        std::vector<AbstractAttribute*> baseAttributes;
        InternalState* internalState;
    };

public:
    AttributeResolver(Context context, ErrorHandler* errorHandler);

    void resolve();

private:
    void resolveOmittedBaseAttributes();

    void resolveOwnAttributes();

    Attribute* resolveOwnAttribute(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    Attribute* resolveBooleanAttribute(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    Attribute* resolveStringAttribute(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    Attribute* resolveNumberAttribute(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    bool resolveNumericSubtype(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    bool resolveNumericRange(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    bool resolveUnit(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    Attribute* resolveEnumerationAttribute(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    Attribute* resolveColorAttribute(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    Attribute* resolveObjectAttribute(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

    void fillAttributeCandidateList(
        const std::vector<nx::vms::api::analytics::AttributeDescription>& attributes,
        const std::map<
            QString /*attributeListId*/,
            std::vector<nx::vms::api::analytics::AttributeDescription>>& attributeLists,
        std::vector<nx::vms::api::analytics::AttributeDescription>* outAttributeCandidateList,
        const std::set<QString>& uniqueAttributeListIds);

    Attribute* tryToDeduceTypeFromSubtype(
        nx::vms::api::analytics::AttributeDescription* inOutAttributeDescription,
        const AbstractAttribute* baseAttribute);

private:
    Context m_context;
    ErrorHandler* m_errorHandler = nullptr;
    std::map<QString, AbstractAttribute*> m_baseAttributeByName;
};

} // namespace nx::analytics::taxonomy
