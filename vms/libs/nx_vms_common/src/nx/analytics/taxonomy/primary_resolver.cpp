// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "primary_resolver.h"

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/analytics/taxonomy/common.h>
#include <nx/analytics/taxonomy/error_handler.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

template<typename T, typename = void>
struct HasBaseItems: std::false_type {};

template<typename T>
struct HasBaseItems<T, std::void_t<decltype(std::declval<T>().baseItems)>>: std::true_type {};

template<typename T, typename = void>
struct HasOmittedBaseItems: std::false_type {};

template<typename T>
struct HasOmittedBaseItems<
    T,
    std::void_t<decltype(std::declval<T>().omittedBaseItems)>>: std::true_type {};

template<typename Descriptor>
std::optional<ProcessingError> validateId(const Descriptor& descriptor, const QString& typeName)
{
    if (descriptor.id.isEmpty())
    {
        return ProcessingError{nx::format("%1: id can't be an empty string", typeName)};
    }

    QRegularExpression idRegExp(
        "^([A-Za-z_][\\}\\{\\-A-Za-z0-9_\\.]+\\$)?[A-Za-z_][\\}\\{\\-A-Za-z0-9_\\.]+$");

    if (const auto match = idRegExp.match(descriptor.id); !match.hasMatch())
    {
        return ProcessingError{nx::format(
            "%1: id string can contain only latin letters, numbers, periods and underscores "
            "and start with a latin letter or an underscore. Given: %2",
            typeName, descriptor.id)};
    }

    return std::nullopt;
}

template<typename Descriptor>
std::optional<ProcessingError> validateFlags(const Descriptor& descriptor)
{
    if constexpr (std::is_same_v<Descriptor, ObjectTypeDescriptor>)
    {
        if (descriptor.flags.testFlag(ObjectTypeFlag::hiddenDerivedType) &&
            (!descriptor.base || descriptor.base->isEmpty()))
        {
            return ProcessingError{nx::format(
                "%1: \"hiddenDerivedType\" flag is available only for derived types. The \"base\" "
                "field must be filled.", descriptor.id)};
        }
    }

    return std::nullopt;
}

template<typename DescriptorMap>
void filterInvalidDescriptors(
    const QString& descriptorTypeName,
    DescriptorMap* inOutDescriptorMap,
    ErrorHandler* errorHandler)
{
    for (auto itr = inOutDescriptorMap->cbegin(); itr != inOutDescriptorMap->cend();)
    {
        std::optional<ProcessingError> error = validateId(itr->second, descriptorTypeName);
        if (!error)
            error = validateFlags(itr->second);

        if (error)
        {
            errorHandler->handleError(*error);
            itr = inOutDescriptorMap->erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

template<typename DescriptorMap>
void resolveInheritance(
    const QString& descriptorTypeName,
    DescriptorMap* inOutDescriptorMap,
    ErrorHandler* errorHandler)
{
    using Descriptor = typename DescriptorMap::mapped_type;

    std::set<QString> processedDescriptorIds;
    for (auto& [id, descriptor] : *inOutDescriptorMap)
    {
        if (processedDescriptorIds.find(id) != processedDescriptorIds.cend())
            continue;

        auto* currentDescriptor = &descriptor;
        std::set<QString> bases;

        if (currentDescriptor->base && currentDescriptor->base->isEmpty())
            currentDescriptor->base.reset();

        while (currentDescriptor->base)
        {
            bases.insert(currentDescriptor->id);

            // If base descriptor of current descriptor is not in inOutDescriptorMap,
            // remove current descriptor base type/base items/omitted base items (if they present).
            auto baseItr = inOutDescriptorMap->find(*currentDescriptor->base);
            if (baseItr == inOutDescriptorMap->cend())
            {
                errorHandler->handleError(
                    ProcessingError{
                        nx::format("%1 %2: missing base type (%3)",
                            descriptorTypeName,
                            currentDescriptor->id,
                            *currentDescriptor->base) });

                currentDescriptor->base = std::nullopt;

                if constexpr (HasBaseItems<Descriptor>::value)
                    currentDescriptor->baseItems.clear();

                if constexpr (HasOmittedBaseItems<Descriptor>::value)
                    currentDescriptor->omittedBaseItems.clear();

                break;
            }

            // If there is a cycle in a base types list,
            // remove current descriptor base type/base items/omitted base items (if they present).
            if (bases.find(*(currentDescriptor->base)) != bases.cend())
            {
                errorHandler->handleError(
                    ProcessingError{
                        nx::format("%1 %2: inheritance cycle (%3)",
                            descriptorTypeName,
                            descriptor.id,
                            containerString(bases)) });

                currentDescriptor->base = std::nullopt;
                if constexpr (HasBaseItems<Descriptor>::value)
                    currentDescriptor->baseItems.clear();

                if constexpr (HasOmittedBaseItems<Descriptor>::value)
                    currentDescriptor->omittedBaseItems.clear();

                break;
            }

            processedDescriptorIds.insert(currentDescriptor->id);
            currentDescriptor = &baseItr->second;
        }
    }
}

void PrimaryResolver::resolve(
    Descriptors* inOutDescriptors,
    ErrorHandler* errorHandler)
{
    filterInvalidDescriptors(
        kGroupDescriptorTypeName, &inOutDescriptors->groupDescriptors, errorHandler);
    filterInvalidDescriptors(
        kEnumTypeDescriptorTypeName, &inOutDescriptors->enumTypeDescriptors, errorHandler);
    filterInvalidDescriptors(
        kColorTypeDescriptorTypeName, &inOutDescriptors->colorTypeDescriptors, errorHandler);
    filterInvalidDescriptors(
        kObjectTypeDescriptorTypeName, &inOutDescriptors->objectTypeDescriptors, errorHandler);
    filterInvalidDescriptors(
        kEventTypeDescriptorTypeName, &inOutDescriptors->eventTypeDescriptors, errorHandler);

    resolveInheritance(
        kEnumTypeDescriptorTypeName, &inOutDescriptors->enumTypeDescriptors, errorHandler);
    resolveInheritance(
        kColorTypeDescriptorTypeName, &inOutDescriptors->colorTypeDescriptors, errorHandler);
    resolveInheritance(
        kObjectTypeDescriptorTypeName, &inOutDescriptors->objectTypeDescriptors, errorHandler);
    resolveInheritance(
        kEventTypeDescriptorTypeName, &inOutDescriptors->eventTypeDescriptors, errorHandler);
}

} // namespace nx::analytics::taxonomy
