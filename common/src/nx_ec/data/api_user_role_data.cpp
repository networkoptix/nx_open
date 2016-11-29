#include "api_user_role_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiUserRoleData)(ApiPredefinedRoleData), (ubjson)(xml)(json)(sql_record)(csv_record)(eq), _Fields)

bool ApiUserRoleData::isNull() const
{
    return id.isNull();
}

} // namespace ec2
