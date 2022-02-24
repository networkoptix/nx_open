// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_reply.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TimeReply, (ubjson)(json)(xml)(sql_record)(csv_record), TimeReply_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ServerTimeReply, (ubjson)(json)(xml)(sql_record)(csv_record), ServerTimeReply_Fields)

} // namespace nx::vms::api
