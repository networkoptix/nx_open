// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

const QString VideowallData::kResourceTypeName = lit("Videowall");
const nx::Uuid VideowallData::kResourceTypeId =
    ResourceData::getFixedTypeId(VideowallData::kResourceTypeName);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    VideowallItemData, (ubjson)(xml)(json)(sql_record)(csv_record), VideowallItemData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VideowallScreenData,
    (ubjson)(xml)(json)(sql_record)(csv_record), VideowallScreenData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VideowallMatrixItemData,
    (ubjson)(xml)(json)(sql_record)(csv_record), VideowallMatrixItemData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VideowallMatrixData,
    (ubjson)(xml)(json)(sql_record)(csv_record), VideowallMatrixData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    VideowallData, (ubjson)(xml)(json)(sql_record)(csv_record), VideowallData_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    VideowallControlMessageData, (ubjson)(json), VideowallControlMessageData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
