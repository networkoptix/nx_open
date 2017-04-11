#include "api_user_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

constexpr const char* ApiUserData::kCloudPasswordStub;

bool ApiUserData::operator==(const ApiUserData& rhs) const
{
    if (!ApiResourceData::operator==(rhs))
        return false;

    return isAdmin == rhs.isAdmin
        && permissions == rhs.permissions
        && userRoleId == rhs.userRoleId
        && email == rhs.email
        && digest == rhs.digest
        && hash == rhs.hash
        && cryptSha512Hash == rhs.cryptSha512Hash
        && realm == rhs.realm
        && isLdap == rhs.isLdap
        && isEnabled == rhs.isEnabled
        && isCloud == rhs.isCloud
        && fullName == rhs.fullName;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiUserData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

} // namespace ec2
