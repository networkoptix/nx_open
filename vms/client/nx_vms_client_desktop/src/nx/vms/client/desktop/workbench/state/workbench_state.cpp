// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_state.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::common {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ResourceDescriptor, (json), ResourceDescriptor_Fields, (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    LayoutItemData, (json), WorkbenchStateUnsavedLayoutItem_Fields);

} // namespace nx::vms::common

namespace nx::vms::client::desktop {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(WorkbenchState::UnsavedLayout, (json),
    WorkbenchStateUnsavedLayout_Fields, (brief, true));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(WorkbenchState, (json), WorkbenchState_Fields, (brief, true));

} // namespace nx::vms::client::desktop
