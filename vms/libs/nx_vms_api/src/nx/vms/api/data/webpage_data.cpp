// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

const QString WebPageData::kResourceTypeName = lit("WebPage");
const QnUuid WebPageData::kResourceTypeId =
    ResourceData::getFixedTypeId(WebPageData::kResourceTypeName);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    WebPageData, (ubjson)(xml)(json)(sql_record)(csv_record), WebPageData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
