#include "user_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

const QString UserData::kResourceTypeName = lit("User");
const QnUuid UserData::kResourceTypeId =
    ResourceData::getFixedTypeId(UserData::kResourceTypeName);

constexpr const char* UserData::kCloudPasswordStub;

void UserData::fillId()
{
    // ATTENTION: This logic is similar to QnUserResource::fillId().
    if (isCloud)
    {
        if (!email.isEmpty())
            id = QnUuid::fromArbitraryData(email);
        else
            id = QnUuid();
    }
    else
    {
        id = QnUuid::createUuid();
    }
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (UserData),
    (eq)(ubjson)(json)(xml)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
