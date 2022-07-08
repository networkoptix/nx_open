// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "network_issue_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NetworkIssueInfo, (json), NetworkIssueInfo_Fields)

bool NetworkIssueInfo::isPrimaryStream() const
{
    return stream == nx::vms::api::StreamIndex::primary;
}

} // namespace nx::vms::rules
