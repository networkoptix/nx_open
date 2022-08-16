// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "descriptors.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

template<typename T, typename = void>
struct HasSupportFlag: std::false_type {};

template<typename T>
struct HasSupportFlag<T, std::void_t<decltype(std::declval<T>().hasEverBeenSupported)>>:
    std::true_type {};

template<typename T, typename = void>
struct HasAttributeSupportInfo: std::false_type {};

template<typename T>
struct HasAttributeSupportInfo<T, std::void_t<decltype(std::declval<T>().attributeSupportInfo)>>:
    std::true_type {};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DescriptorScope,
    (json),
    nx_vms_api_analytics_DescriptorScope_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BaseDescriptor,
    (json),
    nx_vms_api_analytics_BaseDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BaseScopedDescriptor,
    (json),
    nx_vms_api_analytics_BaseScopedDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ExtendedScopedDescriptor,
    (json),
    nx_vms_api_analytics_ExtendedScopedDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PluginDescriptor,
    (json),
    nx_vms_api_analyitcs_PluginDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EngineDescriptor,
    (json),
    nx_vms_api_analytics_EngineDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    GroupDescriptor,
    (json),
    nx_vms_api_analyitcs_GroupDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EventTypeDescriptor,
    (json),
    nx_vms_api_analyitcs_EventTypeDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectTypeDescriptor,
    (json),
    nx_vms_api_analyitcs_ObjectTypeDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ActionTypeDescriptor,
    (json),
    nx_vms_api_analytics_ActionTypeDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EnumTypeDescriptor,
    (json),
    nx_vms_api_analytics_EnumTypeDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ColorTypeDescriptor,
    (json),
    nx_vms_api_analytics_ColorTypeDescriptor_Fields,
    (brief, true))

template<typename Descriptor>
void mergeDescriptors(Descriptor* first, Descriptor* second)
{
    if constexpr (HasSupportFlag<Descriptor>::value)
        second->hasEverBeenSupported |= first->hasEverBeenSupported;

    if constexpr (HasAttributeSupportInfo<Descriptor>::value)
    {
        for (auto& [attributeId, attributeSupportInfo]: first->attributeSupportInfo)
        {
            for (auto& [engineId, deviceIds]: attributeSupportInfo)
            {
                second->attributeSupportInfo[attributeId][engineId].insert(
                    std::make_move_iterator(deviceIds.begin()),
                    std::make_move_iterator(deviceIds.end()));
            }
        }
    }

    for (auto& scope: first->scopes)
    {
        if (auto it = second->scopes.find(scope); it != second->scopes.end())
        {
            it->deviceIds.insert(
                std::make_move_iterator(scope.deviceIds.begin()),
                std::make_move_iterator(scope.deviceIds.end()));
        }
        else
        {
            second->scopes.insert(std::move(scope));
        }
    }

    *first = std::move(*second);
}

void Descriptors::merge(Descriptors descriptors)
{
    for (auto& [id, descriptor]: descriptors.pluginDescriptors)
        this->pluginDescriptors[id] = std::move(descriptor);

    for (auto& [id, descriptor]: descriptors.engineDescriptors)
        this->engineDescriptors[id] = std::move(descriptor);

    for (auto& [id, descriptor]: descriptors.enumTypeDescriptors)
        this->enumTypeDescriptors[id] = std::move(descriptor);

    for (auto& [id, descriptor]: descriptors.colorTypeDescriptors)
        this->colorTypeDescriptors[id] = std::move(descriptor);

    for (auto& [id, descriptor]: descriptors.attributeListDescriptors)
        this->attributeListDescriptors[id] = std::move(descriptor);

    for (auto& [id, descriptor]: descriptors.groupDescriptors)
        mergeDescriptors(&this->groupDescriptors[id], &descriptor);

    for (auto& [id, descriptor]: descriptors.eventTypeDescriptors)
        mergeDescriptors(&this->eventTypeDescriptors[id], &descriptor);

    for (auto& [id, descriptor]: descriptors.objectTypeDescriptors)
        mergeDescriptors(&this->objectTypeDescriptors[id], &descriptor);
}

bool Descriptors::isEmpty() const
{
    return pluginDescriptors.empty()
        && engineDescriptors.empty()
        && groupDescriptors.empty()
        && eventTypeDescriptors.empty()
        && objectTypeDescriptors.empty()
        && colorTypeDescriptors.empty()
        && enumTypeDescriptors.empty()
        && attributeListDescriptors.empty();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Descriptors,
    (json),
    nx_vms_api_analytics_Descriptors_Fields,
    (brief, true))

} // namespace nx::vms::api::analytics
