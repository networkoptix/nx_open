// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "merge_status_reply.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    MergeStatusReply, (ubjson)(json)(xml)(sql_record)(csv_record), MergeStatusReply_Fields)

} // namespace nx::vms::api
