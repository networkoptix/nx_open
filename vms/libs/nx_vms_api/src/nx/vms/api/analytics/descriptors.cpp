#include "descriptors.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    DescriptorScope,
    (json)(eq),
    nx_vms_api_analytics_DescriptorScope_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BaseDescriptor,
    (json)(eq),
    nx_vms_api_analytics_BaseDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BaseScopedDescriptor,
    (json)(eq),
    nx_vms_api_analytics_BaseScopedDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PluginDescriptor,
    (json)(eq),
    nx_vms_api_analyitcs_PluginDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EngineDescriptor,
    (json)(eq),
    nx_vms_api_analytics_EngineDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    GroupDescriptor,
    (json)(eq),
    nx_vms_api_analyitcs_GroupDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EventTypeDescriptor,
    (json)(eq),
    nx_vms_api_analyitcs_EventTypeDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectTypeDescriptor,
    (json)(eq),
    nx_vms_api_analyitcs_ObjectTypeDescriptor_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ActionTypeDescriptor,
    (json)(eq),
    nx_vms_api_analytics_ActionTypeDescriptor_Fields,
    (brief, true))

} // namespace nx::vms::api::analytics
