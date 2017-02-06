#include "api_resource_data.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

bool ApiResourceData::operator==(const ApiResourceData& rhs) const
{
    if (!ApiIdData::operator==(rhs))
        return false;

    return parentId == rhs.parentId
        && name == rhs.name
        && url == rhs.url
        && typeId == rhs.typeId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiResourceParamData)(ApiResourceParamWithRefData)(ApiResourceData)(ApiResourceStatusData),
    (ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace ec2
