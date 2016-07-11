#include "api_user_group_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiUserGroupData)(ApiPredefinedRoleData), (ubjson)(xml)(json)(sql_record)(csv_record)(eq), _Fields)

} // namespace ec2
