#include "api_user_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

constexpr const char* ApiUserData::kCloudPasswordStub;

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiUserData), (eq)(ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

} // namespace ec2
