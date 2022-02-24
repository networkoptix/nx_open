// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

bool LicenseData::operator<(const LicenseData& other) const
{
    return key < other.key;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    LicenseKey, (ubjson)(xml)(json)(sql_record)(csv_record), LicenseKey_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    LicenseData, (ubjson)(xml)(json)(sql_record)(csv_record), LicenseData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DetailedLicenseData,
    (ubjson)(xml)(json)(sql_record)(csv_record), DetailedLicenseData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LicenseSummaryData,
    (ubjson)(xml)(json)(sql_record)(csv_record), LicenseSummaryData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
