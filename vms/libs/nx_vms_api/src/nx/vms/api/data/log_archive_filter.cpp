// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_archive_filter.h"

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/ranges.h>

namespace nx::vms::api {

const std::vector<api::LogName> LogArchiveFilter::kAllNames =
    nx::reflect::enumeration::visitAllItems<api::LogName>(
        []<typename ...T>(T&&... items)
        {
            return std::vector{items.value...}
                | nx::ranges::sort();
        });

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LogArchiveFilter, (json), LogArchiveFilter_Fields)

} // namespace nx::vms::api
