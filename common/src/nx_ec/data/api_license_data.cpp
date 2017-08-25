#include "api_license_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiLicenseData)(ApiDetailedLicenseData), (ubjson)(xml)(json)(sql_record)(csv_record)(eq), _Fields)

bool ApiLicenseData::operator<(const ApiLicenseData& other) const { return key < other.key; }

} // namespace ec2
