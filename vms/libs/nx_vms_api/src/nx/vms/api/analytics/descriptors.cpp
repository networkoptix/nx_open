#include "descriptors.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    HierarchyPath,
    (json),
    nx_vms_api_analytics_HierarchyPath_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PluginDescriptor,
    (json),
    nx_vms_api_analyitcs_PluginDescriptor_Fields,
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

} // namespace nx::vms::api::analytics
