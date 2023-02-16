// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "full_info_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FullInfoData, (ubjson)(json)(xml)(csv_record), FullInfoData_Fields)

DeprecatedFieldNames* FullInfoData::getDeprecatedFieldNames()
{
    static DeprecatedFieldNames kDeprecatedFieldNames{
        {"showreels", "layoutTours"}, //< Up to v5.1.
    };
    return &kDeprecatedFieldNames;
}

} // namespace api
} // namespace vms
} // namespace nx
