// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::utils::media::sdk_support {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MotionData, (json), (channel)(durationMs)(data))

} // namespace nx::utils::media::sdk_support
